/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "clockwork.h"
#include "vm.h"

#include <zmq.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <netdb.h>
#include <getopt.h>
#include <fcntl.h>

#include "policy.h"

#define MODE_RUN  0
#define MODE_DUMP 1
#define MODE_CODE 2
#define MODE_ONCE 3

#define DEFAULT_CONFIG_FILE "/etc/clockwork/cogd.conf"
#define MINIMUM_INTERVAL 30
#define MINIMUM_TIMEOUT  5

typedef struct {
	void *zmq;
	void *broadcast;
	void *control;

	char *fqdn;
	char *gatherers;
	char *copydown;
	char *difftool;

	char *cfm_lock;
	char *cfm_killswitch;

	int   mode;
	int   trace;
	int   daemonize;
	int   acl_default;
	char *acl_file;

	struct {
		char *endpoint;
		char *cert_file;
		cert_t *cert;
	} masters[8];
	int nmasters;
	int current_master;
	int timeout;

	cert_t *cert;
	void *zap;

	list_t *acl;
	hash_t *facts;

	struct {
		int64_t next_run;
		int     interval;
	} schedule;
} client_t;

static pdu_t *s_sendto(void *socket, pdu_t *pdu, int timeout)
{
	if (pdu_send(pdu, socket) != 0) {
		logger(LOG_ERR, "Failed to send %s PDU to remote peer: %s",
			pdu_type(pdu), zmq_strerror(errno));
		return NULL;
	}

	zmq_pollitem_t socks[] = {{ socket, 0, ZMQ_POLLIN, 0 }};
	for (;;) {
		int rc = zmq_poll(socks, 1, timeout);
		if (rc == 1)
			return pdu_recv(socket);
		if (errno != EINTR) break;
	}
	return NULL;
}

static void *s_connect(client_t *c)
{
	void *client = zmq_socket(c->zmq, ZMQ_DEALER);
	if (!client) return NULL;

	int rc;
	logger(LOG_DEBUG, "Setting ZMQ_CURVE_SECRETKEY (sec) to %s", c->cert->seckey_b16);
	rc = zmq_setsockopt(client, ZMQ_CURVE_SECRETKEY, cert_secret(c->cert), 32);
	if (rc != 0) {
		logger(LOG_CRIT, "Failed to set ZMQ_CURVE_SECRETKEY option on client socket: %s",
			zmq_strerror(errno));
		if (errno == EINVAL) {
			int maj, min, rev;
			zmq_version(&maj, &min, &rev);
			logger(LOG_CRIT, "Perhaps the local libzmq (%u.%u.%u) doesn't support CURVE security?",
				maj, min, rev);
		}
		exit(4);
	}
	logger(LOG_DEBUG, "Setting ZMQ_CURVE_PUBLICKEY (pub) to %s", c->cert->pubkey_b16);
	rc = zmq_setsockopt(client, ZMQ_CURVE_PUBLICKEY, cert_public(c->cert), 32);
	assert(rc == 0);

	pdu_t *ping = pdu_make("PING", 0);
	pdu_extendf(ping, "%lu", CLOCKWORK_PROTOCOL);
	pdu_t *pong;

	char endpoint[256] = "tcp://";
	int i;
	for (i = (c->current_master + 1) % 8; ; i = (i + 1) % 8) {
		if (!c->masters[i].endpoint) continue;

		logger(LOG_DEBUG, "Setting ZMQ_CURVE_SERVERKEY (pub) to %s",
				c->masters[i].cert->pubkey_b16);
		rc = zmq_setsockopt(client, ZMQ_CURVE_SERVERKEY, cert_public(c->masters[i].cert), 32);
		assert(rc == 0);

		strncat(endpoint+6, c->masters[i].endpoint, 249);
		logger(LOG_DEBUG, "Attempting to connect to %s (%s)", endpoint, c->masters[i].endpoint);
		rc = zmq_connect(client, endpoint);
		assert(rc == 0);

		/* send the PING */
		pong = s_sendto(client, ping, c->timeout);
		if (pong) {
			if (strcmp(pdu_type(pong), "PONG") != 0) {
				logger(LOG_ERR, "Unexpected %s response from master %i (%s) - expected PONG",
					pdu_type(pong), i+1, c->masters[i].endpoint);

			} else {
				char *vframe = pdu_string(pong, 1);
				int vers = atoi(vframe);
				free(vframe);

				if (vers != CLOCKWORK_PROTOCOL) {
					logger(LOG_ERR, "Upstream server speaks protocol %i (we want %i)",
					vers, CLOCKWORK_PROTOCOL);
				} else break;
			}

		} else {
			if (errno == 0)
				logger(LOG_ERR, "No response from master %i (%s)",
					i+1, c->masters[i].endpoint);
			else if (errno == EAGAIN)
				logger(LOG_ERR, "No response from master %i (%s): possible certificate mismatch",
					i+1, c->masters[i].endpoint);
			else
				logger(LOG_ERR, "Unexpected error talking to master %i (%s): %s",
					i+1, c->masters[i].endpoint, zmq_strerror(errno));
		}

		if (i == c->current_master) {
			logger(LOG_ERR, "No masters were reachable; giving up");
			vzmq_shutdown(client, 0);
			pdu_free(ping);
			pdu_free(pong);
			return NULL;
		};
	}

	pdu_free(ping);
	pdu_free(pong);

	logger(LOG_DEBUG, "setting current master idx to %i", i);
	c->current_master = i;
	return client;
}

static void s_cfm_run(client_t *c)
{
	stopwatch_t t;
	int rc;
	uint32_t count = 0;
	uint32_t ms_connect    = 0,
	         ms_hello      = 0,
	         ms_preinit    = 0,
	         ms_copydown   = 0,
	         ms_facts      = 0,
	         ms_getpolicy  = 0,
	         ms_parse      = 0,
	         ms_enforce    = 0,
	         ms_cleanup    = 0;

	lock_t lock;
	lock_init(&lock, c->cfm_lock);
	if (c->mode != MODE_CODE) {
		struct stat st;
		if (stat(c->cfm_killswitch, &st) == 0) {
			logger(LOG_WARNING, "Found CFM KILLSWITCH %s, dated %s; skipping.",
				c->cfm_killswitch, time_strf("%b %d %Y %H:%M:%S%z", st.st_mtime));
			goto maybe_next_time;
		}

		logger(LOG_DEBUG, "acquiring CFM lock '%s'", lock.path);
		if (lock_acquire(&lock, 0) != 0) {
			logger(LOG_WARNING, "Another configuration management run (%s) in progress; skipping.", lock_info(&lock));
			goto maybe_next_time;
		}
	}

	logger(LOG_NOTICE, "Starting configuration run");
	logger(LOG_INFO, "run was scheduled to start at %s",
		time_strf(NULL, c->schedule.next_run / 1000));

	logger(LOG_DEBUG, "connecting to one of the masters");
	void *client = NULL;
	STOPWATCH(&t, ms_connect) { client = s_connect(c); }

	if (!client)
		goto maybe_next_time;
	logger(LOG_DEBUG, "connected");

	pdu_t *pdu = pdu_make("HELLO", 1, c->fqdn),
	    *reply = NULL;
	STOPWATCH(&t, ms_hello) {
		reply = s_sendto(client, pdu, c->timeout);
		pdu_free(pdu);
	}
	if (!reply) {
		logger(LOG_ERR, "HELLO failed: %s", zmq_strerror(errno));
		goto shut_it_down;
	}
	logger(LOG_DEBUG, "Received a '%s' PDU", pdu_type(reply));
	if (strcmp(pdu_type(reply), "ERROR") == 0) {
		char *e = pdu_string(reply, 1);
		logger(LOG_ERR, "protocol error: %s", e);
		free(e);
		pdu_free(reply);
		goto shut_it_down;
	}
	pdu_free(reply);

	pdu = pdu_make("COPYDOWN", 0);
	STOPWATCH(&t, ms_preinit) {
		reply = s_sendto(client, pdu, c->timeout);
		pdu_free(pdu);
	}
	if (!reply) {
		logger(LOG_ERR, "COPYDOWN failed: %s", zmq_strerror(errno));
		goto shut_it_down;
	}
	logger(LOG_INFO, "COPYDOWN took %lums", stopwatch_ms(&t));
	pdu_free(reply);

	STOPWATCH(&t, ms_copydown) {
		FILE *bdfa = tmpfile();
		int n = 0;
		char size[16];
		for (;;) {
			rc = snprintf(size, 15, "%i", n++);
			assert(rc > 0);
			pdu = pdu_make("DATA", 1, size);
			rc = pdu_send_and_free(pdu, client);
			assert(rc == 0);

			reply = pdu_recv(client);
			if (!reply) {
				logger(LOG_ERR, "DATA failed: %s", zmq_strerror(errno));
				break;
			}
			logger(LOG_DEBUG, "received a %s PDU", pdu_type(reply));

			if (strcmp(pdu_type(reply), "EOF") == 0) {
				pdu_free(reply);
				break;
			}
			if (strcmp(pdu_type(reply), "BLOCK") != 0) {
				logger(LOG_ERR, "protocol violation: received a %s PDU (expected a BLOCK)", pdu_type(reply));
				pdu_free(reply);
				goto shut_it_down;
			}

			size_t len;
			char *data = (char*)pdu_segment(reply, 1, &len);
			fwrite(data, 1, len, bdfa);
			free(data);
			pdu_free(reply);
		}
		rewind(bdfa);
		mkdir(c->copydown, 0777);
		if (cw_bdfa_unpack(fileno(bdfa), c->copydown) != 0) {
			logger(LOG_CRIT, "Unable to perform copydown to %s", c->copydown);
			fclose(bdfa);
			goto shut_it_down;
		}
		fclose(bdfa);
	}

	hash_done(c->facts, 1);
	c->facts = vmalloc(sizeof(hash_t));
	STOPWATCH(&t, ms_facts) {
		logger(LOG_INFO, "Gathering facts from '%s'", c->gatherers);
		rc = fact_gather(c->gatherers, c->facts) != 0;
	}
	if (rc != 0) {
		logger(LOG_CRIT, "Unable to gather facts from %s", c->gatherers);
		goto shut_it_down;
	}

	FILE *io = tmpfile();
	assert(io);

	rc = fact_write(io, c->facts);
	assert(rc == 0);

	fprintf(io, "%c", '\0');
	size_t len = ftell(io);
	fseek(io, 0, SEEK_SET);

	char *factstr = mmap(NULL, len, PROT_READ, MAP_SHARED, fileno(io), 0);
	if ((void *)factstr == MAP_FAILED) {
		logger(LOG_CRIT, "Failed to mmap fact data");
		fclose(io);
		goto shut_it_down;
	}

	pdu = pdu_make("POLICY", 2, c->fqdn, factstr);
	STOPWATCH(&t, ms_getpolicy) {
		reply = s_sendto(client, pdu, c->timeout);
		pdu_free(pdu);
	}

	munmap(factstr, len);
	fclose(io);

	if (!reply) {
		logger(LOG_ERR, "POLICY failed: %s", zmq_strerror(errno));
		goto shut_it_down;
	}
	logger(LOG_DEBUG, "Received a '%s' PDU", pdu_type(reply));
	if (strcmp(pdu_type(reply), "ERROR") == 0) {
		char *e = pdu_string(reply, 1);
		logger(LOG_ERR, "protocol error: %s", e);
		free(e);
		pdu_free(reply);
		goto shut_it_down;
	}

	size_t n;
	uint8_t *code = pdu_segment(reply, 1, &n);
	pdu_free(reply);

	if (c->mode == MODE_CODE) {
		logger(LOG_DEBUG, "dumping pendulum code to standard output");
		vm_disasm(code, n);
		free(code);

	} else {
		vm_t vm;
		STOPWATCH(&t, ms_parse) {
			rc = vm_reset(&vm);
			assert(rc == 0);

			rc = vm_prime(&vm, code, n);
			assert(rc == 0);

			hash_set(&vm.pragma, "diff.tool", c->difftool);
		}
		STOPWATCH(&t, ms_enforce) {
			vm.trace = c->trace;
			vm_exec(&vm);
		}
		count = 0; /* FIXME: count number of topics! */
		rc = vm_done(&vm);
		assert(rc == 0);
	}

	STOPWATCH(&t, ms_cleanup) {
		pdu = pdu_make("BYE", 0);
		reply = s_sendto(client, pdu, c->timeout);
		pdu_free(pdu);
	}
	if (!reply) {
		logger(LOG_ERR, "BYE failed: %s", zmq_strerror(errno));
		goto shut_it_down;
	}
	logger(LOG_DEBUG, "Received a '%s' PDU", pdu_type(reply));
	if (strcmp(pdu_type(reply), "ERROR") == 0) {
		char *e = pdu_string(reply, 1);
		logger(LOG_ERR, "protocol error: %s", e);
		free(e);
	}
	pdu_free(reply);

shut_it_down:
	if (client) {
		vzmq_shutdown(client, 0);
		logger(LOG_INFO, "closed connection");
	}

	logger(LOG_NOTICE, "complete. enforced %lu resources in %0.2lfs",
		count,
		(ms_connect  + ms_hello   + ms_preinit +
		 ms_copydown + ms_facts   + ms_getpolicy +
		 ms_parse    + ms_enforce + ms_cleanup) / 1000.0);

	logger(LOG_NOTICE, "STATS(ms): connect=%u, "   "hello=%u, "    "preinit=%u, "
	                              "copydown=%u, "  "facts=%u, "    "getpolicy=%u, "
	                              "parse=%u, "     "enforce=%u, "  "cleanup=%u",
	                               ms_connect,      ms_hello,       ms_preinit,
	                               ms_copydown,     ms_facts,       ms_getpolicy,
	                               ms_parse,        ms_enforce,     ms_cleanup);

	acl_write(c->acl, c->acl_file);

maybe_next_time:
	lock_release(&lock);

	if (c->mode == MODE_ONCE || c->mode == MODE_CODE)
		return;

	c->schedule.next_run = time_ms() + c->schedule.interval;
	logger(LOG_INFO, "Scheduled next configuration run at %s (every %li %s)",
		time_strf(NULL, c->schedule.next_run / 1000),
		c->schedule.interval / (c->schedule.interval > 120000 ? 60000 : 1000),
		(c->schedule.interval > 120000 ? "minutes" : "seconds"));
}

static inline client_t* s_client_new(int argc, char **argv)
{
	char *s;
	client_t *c = vmalloc(sizeof(client_t));
	c->daemonize = 1;

	logger(LOG_DEBUG, "processing command-line options");
	const char *short_opts = "h?vqVc:FdTX1";
	struct option long_opts[] = {
		{ "help",        no_argument,       NULL, 'h' },
		{ "verbose",     no_argument,       NULL, 'v' },
		{ "quiet",       no_argument,       NULL, 'q' },
		{ "version",     no_argument,       NULL, 'V' },
		{ "config",      required_argument, NULL, 'c' },
		{ "foreground",  no_argument,       NULL, 'F' },
		{ "show-config", no_argument,       NULL, 'd' },
		{ "trace",       no_argument,       NULL, 'T' },
		{ "code",        no_argument,       NULL, 'X' },
		{ "once",        no_argument,       NULL, '1' },
		{ 0, 0, 0, 0 },
	};
	int verbose = 0;
	const char *config_file = DEFAULT_CONFIG_FILE;
	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			logger(LOG_DEBUG, "handling -h/-?/--help");
			logger(LOG_DEBUG, "handling -h/-?/--help");
			printf("cogd, part of clockwork v%s runtime %s protocol %i\n",
				CLOCKWORK_VERSION, CLOCKWORK_RUNTIME, CLOCKWORK_PROTOCOL);
			printf("Usage: cogd [-?hvVqFdT] [-c filename]\n\n");
			printf("Options:\n");
			printf("  -?, -h               show this help screen\n");
			printf("  -V, --version        show version information and exit\n");
			printf("  -v, --verbose        increase logging verbosity\n");
			printf("  -q, --quiet          disable logging\n");
			printf("  -F, --foreground     don't daemonize, run in the foreground\n");
			printf("  -S, --show-config    print configuration and exit\n");
			printf("  -T, --trace          enable TRACE mode on the pendulum runtime\n");
			printf("  -c filename          set configuration file (default: " DEFAULT_CONFIG_FILE ")\n");
			printf("  -X, --code           retrieve Pendulum code, dump it and exit\n");
			printf("  -1, --once           only run once (implies -F)\n");
			exit(0);
			break;

		case 'v':
			if (verbose < 0) verbose = 0;
			verbose++;
			logger(LOG_DEBUG, "handling -v/--verbose (modifier = %i)", verbose);
			break;

		case 'q':
			verbose = -1 * LOG_DEBUG;;
			logger(LOG_DEBUG, "handling -q/--quiet (modifier = %i)", verbose);
			break;

		case 'V':
			logger(LOG_DEBUG, "handling -V/--version");
			printf("cogd (Clockwork) %s runtime %s\n"
			       "Copyright (C) 2014 James Hunt\n",
			       CLOCKWORK_VERSION, CLOCKWORK_RUNTIME);
			exit(0);
			break;

		case 'c':
			logger(LOG_DEBUG, "handling -c/--config; replacing '%s' with '%s'",
				config_file, optarg);
			config_file = optarg;
			break;

		case 'F':
			logger(LOG_DEBUG, "handling -F/--foreground; turning off daemonize behavior");
			c->daemonize = 0;
			break;

		case 'd':
			logger(LOG_DEBUG, "handling -S/--show-config");
			c->mode = MODE_DUMP;
			break;

		case 'T':
			logger(LOG_DEBUG, "handling -T/--trace");
			logger(LOG_DEBUG, "TRACE option will be set on all Pendulum runs");
			c->trace = 1;
			break;

		case '1':
			logger(LOG_DEBUG, "handling -1/--once");
			logger(LOG_DEBUG, "Setting implied -F/--foreground for -X/--code");
			c->mode = MODE_ONCE;
			c->daemonize = 0;
			break;

		case 'X':
			logger(LOG_DEBUG, "handling -X/--code");
			logger(LOG_DEBUG, "Setting implied -F/--foreground for -X/--code");
			c->mode = MODE_CODE;
			c->daemonize = 0;
			break;
		}
	}
	logger(LOG_DEBUG, "option processing complete");
	logger(LOG_DEBUG, "running in mode %i\n", c->mode);


	LIST(config);
	config_set(&config, "timeout",         "5");
	config_set(&config, "gatherers",       CW_GATHER_DIR "/*");
	config_set(&config, "copydown",        CW_GATHER_DIR);
	config_set(&config, "interval",        "300");
	config_set(&config, "acl",             "/etc/clockwork/local.acl");
	config_set(&config, "acl.default",     "deny");
	config_set(&config, "syslog.ident",    "cogd");
	config_set(&config, "syslog.facility", "daemon");
	config_set(&config, "syslog.level",    "error");
	config_set(&config, "security.cert",   "/etc/clockwork/certs/cogd");
	config_set(&config, "pidfile",         "/var/run/cogd.pid");
	config_set(&config, "lockdir",         "/var/lock/cogd");
	config_set(&config, "difftool",        "/usr/bin/diff -u");

	log_open(config_get(&config, "syslog.ident"), "stderr");
	log_level(0, (getenv("COGD_DEBUG") ? "debug" : "error"));
	logger(LOG_DEBUG, "default configuration:");
	logger(LOG_DEBUG, "  timeout         %s", config_get(&config, "timeout"));
	logger(LOG_DEBUG, "  gatherers       %s", config_get(&config, "gatherers"));
	logger(LOG_DEBUG, "  copydown        %s", config_get(&config, "copydown"));
	logger(LOG_DEBUG, "  interval        %s", config_get(&config, "interval"));
	logger(LOG_DEBUG, "  acl             %s", config_get(&config, "acl"));
	logger(LOG_DEBUG, "  acl.default     %s", config_get(&config, "acl.default"));
	logger(LOG_DEBUG, "  syslog.ident    %s", config_get(&config, "syslog.ident"));
	logger(LOG_DEBUG, "  syslog.facility %s", config_get(&config, "syslog.facility"));
	logger(LOG_DEBUG, "  syslog.level    %s", config_get(&config, "syslog.level"));
	logger(LOG_DEBUG, "  security.cert   %s", config_get(&config, "security.cert"));
	logger(LOG_DEBUG, "  pidfile         %s", config_get(&config, "pidfile"));
	logger(LOG_DEBUG, "  lockdir         %s", config_get(&config, "lockdir"));
	logger(LOG_DEBUG, "  difftool        %s", config_get(&config, "difftool"));


	logger(LOG_DEBUG, "parsing cogd configuration file '%s'", config_file);
	FILE *io = fopen(config_file, "r");
	if (!io) {
		logger(LOG_WARNING, "Failed to read configuration from %s: %s",
			config_file, strerror(errno));
		logger(LOG_WARNING, "Using default configuration");

	} else {
		if (config_read(&config, io) != 0) {
			logger(LOG_ERR, "Unable to parse %s");
			exit(1);
		}
		fclose(io);
		io = NULL;
	}


	logger(LOG_DEBUG, "determining adjusted log level/facility");
	int level = LOG_NOTICE;
	if (c->daemonize) {
		s = config_get(&config, "syslog.level");
		logger(LOG_DEBUG, "configured log level is '%s', verbose modifier is %+i", s, verbose);
		level = log_level_number(s);

		if (level < 0) {
			logger(LOG_WARNING, "'%s' is not a recognized log level, falling back to 'error'", s);
			level = LOG_ERR;
		}
	}
	level += verbose;
	if (level > LOG_DEBUG) level = LOG_DEBUG;
	if (level < 0) level = 0;
	logger(LOG_DEBUG, "adjusted log level is %s (%i)",
		log_level_name(level), level);
	if (!c->daemonize) {
		logger(LOG_DEBUG, "Running in --foreground mode; forcing all logging to stderr");
		config_set(&config, "syslog.facility", "stderr");
		umask(0);
	}
	if (c->mode == MODE_DUMP) {
		logger(LOG_DEBUG, "Running in --show-config mode; forcing all logging to stderr");
		config_set(&config, "syslog.facility", "stderr");
	}
	logger(LOG_DEBUG, "redirecting to %s log as %s",
		config_get(&config, "syslog.facility"),
		config_get(&config, "syslog.ident"));

	log_open(config_get(&config, "syslog.ident"),
	            config_get(&config, "syslog.facility"));
	log_level(level, NULL);

	logger(LOG_DEBUG, "parsing master.* definitions into endpoint records");
	int n; char key[9] = "master.1", cert[7] = "cert.1";
	c->nmasters = 0;
	memset(c->masters, 0, sizeof(c->masters));
	for (n = 0; n < 8; n++, key[7]++, cert[5]++) {
		logger(LOG_DEBUG, "searching for %s", key);
		s = config_get(&config, key);
		if (!s) break;
		logger(LOG_DEBUG, "found a master: %s", s);
		c->masters[n].endpoint = strdup(s);
		c->nmasters++;

		logger(LOG_DEBUG, "searching for %s", cert);
		s = config_get(&config, cert);
		if (!s) break;
		logger(LOG_DEBUG, "found certificate: %s", s);
		c->masters[n].cert_file = strdup(s);
	}

	c->acl_file = strdup(config_get(&config, "acl"));
	logger(LOG_DEBUG, "parsing stored ACLs from %s", c->acl_file);
	c->acl = vmalloc(sizeof(list_t)); list_init(c->acl);
	acl_read(c->acl, c->acl_file);

	if (c->mode == MODE_DUMP) {
		int i;
		for (i = 0; i < c->nmasters; i++) {
			printf("master.%i        %s\n", i+1, c->masters[i].endpoint);
			if (c->masters[i].cert_file) {
				printf("cert.%i          %s\n", i+1, c->masters[i].cert_file);
			}
		}
		printf("timeout         %s\n", config_get(&config, "timeout"));
		printf("gatherers       %s\n", config_get(&config, "gatherers"));
		printf("copydown        %s\n", config_get(&config, "copydown"));
		printf("interval        %s\n", config_get(&config, "interval"));
		printf("acl             %s\n", config_get(&config, "acl"));
		printf("acl.default     %s\n", config_get(&config, "acl.default"));
		printf("syslog.ident    %s\n", config_get(&config, "syslog.ident"));
		printf("syslog.facility %s\n", config_get(&config, "syslog.facility"));
		printf("syslog.level    %s\n", config_get(&config, "syslog.level"));
		printf("security.cert   %s\n", config_get(&config, "security.cert"));
		printf("pidfile         %s\n", config_get(&config, "pidfile"));
		printf("lockdir         %s\n", config_get(&config, "lockdir"));
		printf("difftool        %s\n", config_get(&config, "difftool"));
		exit(0);
	}


	if (c->nmasters == 0) {
		logger(LOG_ERR, "No masters defined in %s", config_file);
		exit(2);
	}

	int i;
	for (i = 0; i < c->nmasters; i++) {
		if (c->masters[i].cert_file) {
			logger(LOG_DEBUG, "Reading master.%i certificate from %s",
				i+1, c->masters[i].cert_file);
			if ((c->masters[i].cert = cert_read(c->masters[i].cert_file)) != NULL)
				continue;
			logger(LOG_ERR, "cert.%i %s: %s", i+1, c->masters[i].cert_file,
				errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
			exit(2);
		} else {
			logger(LOG_ERR, "master.%i (%s) has no matching certificate (cert.%i)",
				i+1, c->masters[i].endpoint, i+1);
			exit(2);
		}
	}

#ifndef UNIT_TESTS
	if (getuid() != 0 || geteuid() != 0) {
		fprintf(stderr, "%s must be run as root!\n", argv[0]);
		exit(9);
	}
#endif

	s = config_get(&config, "security.cert");
	logger(LOG_DEBUG, "reading public/private key from certificate file %s", s);
	c->cert = cert_read(s);
	if (!c->cert) {
		logger(LOG_ERR, "%s: %s", s,
			errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
		exit(1);
	}
	if (!c->cert->seckey) {
		logger(LOG_ERR, "%s: No secret key found in certificate", s);
		exit(1);
	}
	if (!c->cert->ident) {
		logger(LOG_ERR, "%s: No identity found in certificate", s);
		exit(1);
	}


	logger(LOG_INFO, "cogd starting up");
	c->schedule.next_run = time_ms();

	logger(LOG_DEBUG, "detecting fully-qualified domain name of local node");
	c->fqdn = fqdn();
	if (!c->fqdn) exit(1);
	logger(LOG_INFO, "detected my FQDN as '%s'", c->fqdn);


	c->gatherers = strdup(config_get(&config, "gatherers"));
	c->copydown  = strdup(config_get(&config, "copydown"));
	c->difftool  = strdup(config_get(&config, "difftool"));
	c->schedule.interval  = atoi(config_get(&config, "interval"));
	c->timeout            = atoi(config_get(&config, "timeout"));
	c->acl_default = strcmp(config_get(&config, "acl.default"), "deny") == 0
		? ACL_DENY : ACL_ALLOW;

	/* enforce sane defaults, in case we screwed up the config */
	if (c->schedule.interval < MINIMUM_INTERVAL) {
		logger(LOG_WARNING, "invalid interval value %i detected; "
		                    "falling back to sane default (%i)",
		                    c->schedule.interval, MINIMUM_INTERVAL);
		c->schedule.interval = MINIMUM_INTERVAL;
	}
	if (c->timeout < MINIMUM_TIMEOUT) {
		logger(LOG_WARNING, "invalid timeout value %i detected; "
		                    "falling back to sane default (%i)",
		                    c->timeout, MINIMUM_TIMEOUT);
		c->timeout = MINIMUM_TIMEOUT;
	}

	c->schedule.interval *= 1000;
	c->timeout           *= 1000;

	unsetenv("COGD");
	if (c->daemonize) {
		if (setenv("COGD", "1", 1) != 0) {
			fprintf(stderr, "Failed to set COGD env var: %s\n", strerror(errno));
			exit(3);
		}
		daemonize(config_get(&config, "pidfile"), "root", "root");
	}


	c->zmq = zmq_ctx_new();
	c->zap = zap_startup(c->zmq, NULL);

	if (c->mode == MODE_ONCE || c->mode == MODE_CODE) {
		logger(LOG_DEBUG, "Running under -X/--code or -1/--once; skipping bind");

	} else if (config_get(&config, "mesh.broadcast")
	        && config_get(&config, "mesh.control")
	        && config_get(&config, "mesh.cert")) {

		cert_t *ephemeral = cert_generate(VIGOR_CERT_ENCRYPTION);
		cert_t *mesh_cert = cert_read(config_get(&config, "mesh.cert"));
		if (!mesh_cert) {
			logger(LOG_ERR, "mesh.cert %s: %s", config_get(&config, "mesh.cert"),
				errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
			exit(1);
		}
		if (!mesh_cert->pubkey) {
			logger(LOG_ERR, "No public key found in mesh.cert %s", config_get(&config, "mesh.cert"));
			exit(1);
		}

		char *control   = string("tcp://%s", config_get(&config, "mesh.control"));
		char *broadcast = string("tcp://%s", config_get(&config, "mesh.broadcast"));

		int rc;
		logger(LOG_INFO, "Connecting to the mesh control at %s", control);
		c->control = zmq_socket(c->zmq, ZMQ_DEALER);
		rc = zmq_setsockopt(c->control, ZMQ_CURVE_SECRETKEY, cert_secret(ephemeral), 32);
		assert(rc == 0);
		rc = zmq_setsockopt(c->control, ZMQ_CURVE_PUBLICKEY, cert_public(ephemeral), 32);
		assert(rc == 0);
		rc = zmq_setsockopt(c->control, ZMQ_CURVE_SERVERKEY, cert_public(mesh_cert), 32);
		assert(rc == 0);
		rc = zmq_connect(c->control, control);
		assert(rc == 0);


		logger(LOG_INFO, "Subscribing to the mesh broadcast at %s", broadcast);
		c->broadcast = zmq_socket(c->zmq, ZMQ_SUB);
		rc = zmq_setsockopt(c->broadcast, ZMQ_CURVE_SECRETKEY, cert_secret(ephemeral), 32);
		assert(rc == 0);
		rc = zmq_setsockopt(c->broadcast, ZMQ_CURVE_PUBLICKEY, cert_public(ephemeral), 32);
		assert(rc == 0);
		rc = zmq_setsockopt(c->broadcast, ZMQ_CURVE_SERVERKEY, cert_public(mesh_cert), 32);
		assert(rc == 0);
		rc = zmq_connect(c->broadcast, broadcast);
		assert(rc == 0);
		rc = zmq_setsockopt(c->broadcast, ZMQ_SUBSCRIBE, NULL, 0);
		assert(rc == 0);


		cert_free(ephemeral);

	} else {
		logger(LOG_INFO, "Skipping mesh registeration");
		c->broadcast = c->control = NULL;
	}

	c->cfm_lock = string("%s/%s",
		config_get(&config, "lockdir"),
		"cfm.lock");
	logger(LOG_DEBUG, "will use CFM lock file '%s'", c->cfm_lock);

	c->cfm_killswitch = string("%s/%s",
		config_get(&config, "lockdir"),
		"cfm.KILLSWITCH");
	logger(LOG_DEBUG, "will use CFM killswitch file '%s'", c->cfm_killswitch);

	config_done(&config);
	return c;
}

static void s_client_free(client_t *c)
{
	assert(c);
	int i;
	cert_free(c->cert);
	for (i = 0; i < c->nmasters; i++) {
		free(c->masters[i].endpoint);
		free(c->masters[i].cert_file);
		cert_free(c->masters[i].cert);
	}

	acl_t *a, *a_tmp;
	for_each_object_safe(a, a_tmp, c->acl, l)
		acl_destroy(a);
	free(c->acl);
	free(c->acl_file);

	hash_done(c->facts, 1);
	free(c->facts);

	free(c->gatherers);
	free(c->copydown);
	free(c->difftool);
	free(c->fqdn);
	free(c->cfm_lock);
	free(c->cfm_killswitch);

	if (c->broadcast) {
		logger(LOG_DEBUG, "shutting down mesh broadcast socket");
		vzmq_shutdown(c->broadcast, 0);
	}
	if (c->control) {
		logger(LOG_DEBUG, "shutting down mesh control socket");
		vzmq_shutdown(c->control, 0);
	}
	zap_shutdown(c->zap);
	zmq_ctx_destroy(c->zmq);

	free(c);
}

int main(int argc, char **argv)
{
#ifdef UNIT_TESTS
	/* only let unit tests run for 60s */
	alarm(60);
#endif

	client_t *c = s_client_new(argc, argv);

	if (c->mode == MODE_ONCE || c->mode == MODE_CODE) {
		s_cfm_run(c);
		goto shut_it_down;
	}

	while (!signalled()) {
		zmq_pollitem_t socks[] = {
			{ c->broadcast, 0, ZMQ_POLLIN, 0 },
		};

		int time_left = (int)((c->schedule.next_run - time_ms()));
		if (time_left < 0) time_left = 0;
		logger(LOG_DEBUG, "zmq_poll for %ims", time_left);

		errno = 0;
		int rc = zmq_poll(socks, c->broadcast ? 1 : 0, time_left);
		if (rc == -1)
			break;

		if (socks[0].revents == ZMQ_POLLIN) {
			mesh_client_t *client = mesh_client_new();
			rc = mesh_client_setopt(client, MESH_CLIENT_FACTS, c->facts, sizeof(c->facts));
			assert(rc == 0);

			rc = mesh_client_setopt(client, MESH_CLIENT_ACL, c->acl, sizeof(c->acl));
			assert(rc == 0);

			rc = mesh_client_setopt(client, MESH_CLIENT_ACL_DEFAULT, &c->acl_default, sizeof(c->acl_default));
			assert(rc == 0);

			rc = mesh_client_setopt(client, MESH_CLIENT_FQDN, c->fqdn, strlen(c->fqdn));
			assert(rc == 0);

			rc = mesh_client_setopt(client, MESH_CLIENT_GATHERERS, c->gatherers, strlen(c->gatherers));
			assert(rc == 0);

			pdu_t *pdu = pdu_recv(c->broadcast);
			rc = mesh_client_handle(client, c->control, pdu);
			pdu_free(pdu);

			mesh_client_destroy(client);
		}

		if (time_ms() >= c->schedule.next_run) {
			s_cfm_run(c);
#ifdef UNIT_TESTS
			if (getenv("TEST_COGD_BAIL_EARLY"))
				exit(0);
#endif
		}
	}

shut_it_down:
	logger(LOG_INFO, "shutting down");

	s_client_free(c);
	log_close();
	return 0;
}
