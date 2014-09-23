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
#include "pendulum.h"
#include "pendulum_funcs.h"

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

#define PROTOCOL_VERSION         1
#define PROTOCOL_VERSION_STRING "1"

#define DEFAULT_CONFIG_FILE "/etc/clockwork/cogd.conf"

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
		cw_cert_t *cert;
	} masters[8];
	int nmasters;
	int current_master;
	int timeout;

	cw_cert_t *cert;
	void *zap;

	cw_list_t *acl;
	cw_hash_t *facts;

	struct {
		int64_t next_run;
		int     interval;
	} schedule;
} client_t;

static cw_pdu_t *s_sendto(void *socket, cw_pdu_t *pdu, int timeout)
{
	if (cw_pdu_send(socket, pdu) != 0) {
		cw_log(LOG_ERR, "Failed to send %s PDU to remote peer: %s",
			pdu->type, zmq_strerror(errno));
		return NULL;
	}

	zmq_pollitem_t socks[] = {{ socket, 0, ZMQ_POLLIN, 0 }};
	for (;;) {
		int rc = zmq_poll(socks, 1, timeout);
		if (rc == 1)
			return cw_pdu_recv(socket);
		if (errno != EINTR) break;
	}
	return NULL;
}

static void *s_connect(client_t *c)
{
	void *client = zmq_socket(c->zmq, ZMQ_DEALER);
	if (!client) return NULL;

	int rc;
	cw_log(LOG_DEBUG, "Setting ZMQ_CURVE_SECRETKEY (sec) to %s", c->cert->seckey_b16);
	rc = zmq_setsockopt(client, ZMQ_CURVE_SECRETKEY, cw_cert_secret(c->cert), 32);
	assert(rc == 0);
	cw_log(LOG_DEBUG, "Setting ZMQ_CURVE_PUBLICKEY (pub) to %s", c->cert->pubkey_b16);
	rc = zmq_setsockopt(client, ZMQ_CURVE_PUBLICKEY, cw_cert_public(c->cert), 32);
	assert(rc == 0);

	cw_pdu_t *ping = cw_pdu_make(NULL, 2, "PING", PROTOCOL_VERSION_STRING);
	cw_pdu_t *pong;

	char endpoint[256] = "tcp://";
	int i;
	for (i = (c->current_master + 1) % 8; ; i = (i + 1) % 8) {
		if (!c->masters[i].endpoint) continue;

		cw_log(LOG_DEBUG, "Setting ZMQ_CURVE_SERVERKEY (pub) to %s",
				c->masters[i].cert->pubkey_b16);
		rc = zmq_setsockopt(client, ZMQ_CURVE_SERVERKEY, cw_cert_public(c->masters[i].cert), 32);
		assert(rc == 0);

		strncat(endpoint+6, c->masters[i].endpoint, 249);
		cw_log(LOG_DEBUG, "Attempting to connect to %s (%s)", endpoint, c->masters[i].endpoint);
		rc = zmq_connect(client, endpoint);
		assert(rc == 0);

		/* send the PING */
		pong = s_sendto(client, ping, c->timeout);
		if (pong) {
			if (strcmp(pong->type, "PONG") != 0) {
				cw_log(LOG_ERR, "Unexpected %s response from master %i (%s) - expected PONG",
					pong->type, i+1, c->masters[i].endpoint);

			} else {
				char *vframe = cw_pdu_text(pong, 1);
				int vers = atoi(vframe);
				free(vframe);

				if (vers != PROTOCOL_VERSION) {
					cw_log(LOG_ERR, "Upstream server speaks protocol %i (we want %i)",
					vers, PROTOCOL_VERSION);
				} else break;
			}

		} else {
			if (errno == 0)
				cw_log(LOG_ERR, "No response from master %i (%s)",
					i+1, c->masters[i].endpoint);
			else if (errno == EAGAIN)
				cw_log(LOG_ERR, "No response from master %i (%s): possible certificate mismatch",
					i+1, c->masters[i].endpoint);
			else
				cw_log(LOG_ERR, "Unexpected error talking to master %i (%s): %s",
					i+1, c->masters[i].endpoint, zmq_strerror(errno));
		}

		if (i == c->current_master) {
			cw_log(LOG_ERR, "No masters were reachable; giving up");
			cw_zmq_shutdown(client, 0);
			cw_pdu_destroy(ping);
			cw_pdu_destroy(pong);
			return NULL;
		};
	}

	cw_pdu_destroy(ping);
	cw_pdu_destroy(pong);

	cw_log(LOG_DEBUG, "setting current master idx to %i", i);
	c->current_master = i;
	return client;
}

static void s_cfm_run(client_t *c)
{
	cw_timer_t t;
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

	cw_lock_t lock;
	cw_lock_init(&lock, c->cfm_lock);
	if (c->mode != MODE_CODE) {
		struct stat st;
		if (stat(c->cfm_killswitch, &st) == 0) {
			cw_log(LOG_WARNING, "Found CFM KILLSWITCH %s, dated %s; skipping.",
				c->cfm_killswitch, cw_time_strf("%b %d %Y %H:%M:%S%z", st.st_mtime));
			goto maybe_next_time;
		}

		cw_log(LOG_DEBUG, "acquiring CFM lock '%s'", lock.path);
		if (cw_lock(&lock, 0) != 0) {
			cw_log(LOG_WARNING, "Another configuration management run (%s) in progress; skipping.", cw_lock_info(&lock));
			goto maybe_next_time;
		}
	}

	cw_log(LOG_NOTICE, "Starting configuration run");
	cw_log(LOG_INFO, "run was scheduled to start at %s",
		cw_time_strf(NULL, c->schedule.next_run / 1000));

	cw_log(LOG_DEBUG, "connecting to one of the masters");
	void *client = NULL;
	TIMER(&t, ms_connect) { client = s_connect(c); }

	if (!client)
		goto maybe_next_time;
	cw_log(LOG_DEBUG, "connected");

	cw_pdu_t *pdu, *reply = NULL;

	pdu = cw_pdu_make(NULL, 2, "HELLO", c->fqdn);
	TIMER(&t, ms_hello) {
		reply = s_sendto(client, pdu, c->timeout);
		cw_pdu_destroy(pdu);
	}
	if (!reply) {
		cw_log(LOG_ERR, "HELLO failed: %s", zmq_strerror(errno));
		goto shut_it_down;
	}
	cw_log(LOG_DEBUG, "Received a '%s' PDU", reply->type);
	if (strcmp(reply->type, "ERROR") == 0) {
		char *e = cw_pdu_text(reply, 1);
		cw_log(LOG_ERR, "protocol error: %s", e);
		free(e);
		cw_pdu_destroy(reply);
		goto shut_it_down;
	}
	cw_pdu_destroy(reply);

	pdu = cw_pdu_make(NULL, 1, "COPYDOWN");
	TIMER(&t, ms_preinit) {
		reply = s_sendto(client, pdu, c->timeout);
		cw_pdu_destroy(pdu);
	}
	if (!reply) {
		cw_log(LOG_ERR, "COPYDOWN failed: %s", zmq_strerror(errno));
		goto shut_it_down;
	}
	cw_log(LOG_INFO, "COPYDOWN took %lums", cw_timer_ms(&t));
	cw_pdu_destroy(reply);

	TIMER(&t, ms_copydown) {
		FILE *bdfa = tmpfile();
		int n = 0;
		char size[16];
		for (;;) {
			rc = snprintf(size, 15, "%i", n++);
			assert(rc > 0);
			pdu = cw_pdu_make(NULL, 2, "DATA", size);
			rc = cw_pdu_send(client, pdu);
			assert(rc == 0);
			cw_pdu_destroy(pdu);

			reply = cw_pdu_recv(client);
			if (!reply) {
				cw_log(LOG_ERR, "DATA failed: %s", zmq_strerror(errno));
				break;
			}
			cw_log(LOG_DEBUG, "received a %s PDU", reply->type);

			if (strcmp(reply->type, "EOF") == 0) {
				cw_pdu_destroy(reply);
				break;
			}
			if (strcmp(reply->type, "BLOCK") != 0) {
				cw_log(LOG_ERR, "protocol violation: received a %s PDU (expected a BLOCK)", reply->type);
				cw_pdu_destroy(reply);
				goto shut_it_down;
			}

			char *data = cw_pdu_text(reply, 1);
			fwrite(data, 1, cw_pdu_framelen(reply, 1), bdfa);
			free(data);
			cw_pdu_destroy(reply);
		}
		rewind(bdfa);
		mkdir(c->copydown, 0777);
		if (cw_bdfa_unpack(fileno(bdfa), c->copydown) != 0) {
			cw_log(LOG_CRIT, "Unable to perform copydown to %s", c->copydown);
			fclose(bdfa);
			goto shut_it_down;
		}
		fclose(bdfa);
	}

	cw_hash_done(c->facts, 1);
	c->facts = cw_alloc(sizeof(cw_hash_t));
	TIMER(&t, ms_facts) {
		cw_log(LOG_INFO, "Gathering facts from '%s'", c->gatherers);
		rc = fact_gather(c->gatherers, c->facts) != 0;
	}
	if (rc != 0) {
		cw_log(LOG_CRIT, "Unable to gather facts from %s", c->gatherers);
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
		cw_log(LOG_CRIT, "Failed to mmap fact data");
		fclose(io);
		goto shut_it_down;
	}

	pdu = cw_pdu_make(NULL, 3, "POLICY", c->fqdn, factstr);
	TIMER(&t, ms_getpolicy) {
		reply = s_sendto(client, pdu, c->timeout);
		cw_pdu_destroy(pdu);
	}

	munmap(factstr, len);
	fclose(io);

	if (!reply) {
		cw_log(LOG_ERR, "POLICY failed: %s", zmq_strerror(errno));
		goto shut_it_down;
	}
	cw_log(LOG_DEBUG, "Received a '%s' PDU", reply->type);
	if (strcmp(reply->type, "ERROR") == 0) {
		char *e = cw_pdu_text(reply, 1);
		cw_log(LOG_ERR, "protocol error: %s", e);
		free(e);
		cw_pdu_destroy(reply);
		goto shut_it_down;
	}

	char *code = cw_pdu_text(reply, 1);
	cw_pdu_destroy(reply);

	if (c->mode == MODE_CODE) {
		cw_log(LOG_DEBUG, "dumping pendulum code to standard output");
		printf("%s\n", code);
		free(code);

	} else {
		pn_machine m;
		TIMER(&t, ms_parse) {
			pn_init(&m);
			pendulum_init(&m, client);
			pn_pragma(&m, "diff.tool", c->difftool);

			io = tmpfile();
			fprintf(io, "%s", code);
			fseek(io, 0, SEEK_SET);
			free(code);

			pn_parse(&m, io);
		}
		TIMER(&t, ms_enforce) {
			m.trace = c->trace;
			pn_run(&m);
			fclose(io);
		}
		count = m.topics;

		pendulum_destroy(&m);
		pn_destroy(&m);
	}

	TIMER(&t, ms_cleanup) {
		pdu = cw_pdu_make(NULL, 1, "BYE");
		reply = s_sendto(client, pdu, c->timeout);
		cw_pdu_destroy(pdu);
	}
	if (!reply) {
		cw_log(LOG_ERR, "BYE failed: %s", zmq_strerror(errno));
		goto shut_it_down;
	}
	cw_log(LOG_DEBUG, "Received a '%s' PDU", reply->type);
	if (strcmp(reply->type, "ERROR") == 0) {
		char *e = cw_pdu_text(reply, 1);
		cw_log(LOG_ERR, "protocol error: %s", e);
		free(e);
	}
	cw_pdu_destroy(reply);

shut_it_down:
	if (client) {
		cw_zmq_shutdown(client, 0);
		cw_log(LOG_INFO, "closed connection");
	}

	cw_log(LOG_NOTICE, "complete. enforced %lu resources in %0.2lfs",
		count,
		(ms_connect  + ms_hello   + ms_preinit +
		 ms_copydown + ms_facts   + ms_getpolicy +
		 ms_parse    + ms_enforce + ms_cleanup) / 1000.0);

	cw_log(LOG_NOTICE, "STATS(ms): connect=%u, "   "hello=%u, "    "preinit=%u, "
	                              "copydown=%u, "  "facts=%u, "    "getpolicy=%u, "
	                              "parse=%u, "     "enforce=%u, "  "cleanup=%u",
	                               ms_connect,      ms_hello,       ms_preinit,
	                               ms_copydown,     ms_facts,       ms_getpolicy,
	                               ms_parse,        ms_enforce,     ms_cleanup);

	acl_write(c->acl, c->acl_file);

maybe_next_time:
	cw_unlock(&lock);

	if (c->mode == MODE_ONCE || c->mode == MODE_CODE)
		return;

	c->schedule.next_run = cw_time_ms() + c->schedule.interval;
	cw_log(LOG_INFO, "Scheduled next configuration run at %s (every %li %s)",
		cw_time_strf(NULL, c->schedule.next_run / 1000),
		c->schedule.interval / (c->schedule.interval > 120000 ? 60000 : 1000),
		(c->schedule.interval > 120000 ? "minutes" : "seconds"));
}

static inline client_t* s_client_new(int argc, char **argv)
{
	char *s;
	client_t *c = cw_alloc(sizeof(client_t));
	c->daemonize = 1;

	cw_log(LOG_DEBUG, "processing command-line options");
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
			cw_log(LOG_DEBUG, "handling -h/-?/--help");
			cw_log(LOG_DEBUG, "handling -h/-?/--help");
			printf("cogd, part of clockwork v%s runtime %i protocol %i\n",
				PACKAGE_VERSION, PENDULUM_VERSION, PROTOCOL_VERSION);
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
			cw_log(LOG_DEBUG, "handling -v/--verbose (modifier = %i)", verbose);
			break;

		case 'q':
			verbose = -1 * LOG_DEBUG;;
			cw_log(LOG_DEBUG, "handling -q/--quiet (modifier = %i)", verbose);
			break;

		case 'V':
			cw_log(LOG_DEBUG, "handling -V/--version");
			printf("cogd (Clockwork) %s runtime v%04x\n"
			       "Copyright (C) 2014 James Hunt\n",
			       PACKAGE_VERSION, PENDULUM_VERSION);
			exit(0);
			break;

		case 'c':
			cw_log(LOG_DEBUG, "handling -c/--config; replacing '%s' with '%s'",
				config_file, optarg);
			config_file = optarg;
			break;

		case 'F':
			cw_log(LOG_DEBUG, "handling -F/--foreground; turning off daemonize behavior");
			c->daemonize = 0;
			break;

		case 'd':
			cw_log(LOG_DEBUG, "handling -S/--show-config");
			c->mode = MODE_DUMP;
			break;

		case 'T':
			cw_log(LOG_DEBUG, "handling -T/--trace");
			cw_log(LOG_DEBUG, "TRACE option will be set on all Pendulum runs");
			c->trace = 1;
			break;

		case '1':
			cw_log(LOG_DEBUG, "handling -1/--once");
			cw_log(LOG_DEBUG, "Setting implied -F/--foreground for -X/--code");
			c->mode = MODE_ONCE;
			c->daemonize = 0;
			break;

		case 'X':
			cw_log(LOG_DEBUG, "handling -X/--code");
			cw_log(LOG_DEBUG, "Setting implied -F/--foreground for -X/--code");
			c->mode = MODE_CODE;
			c->daemonize = 0;
			break;
		}
	}
	cw_log(LOG_DEBUG, "option processing complete");
	cw_log(LOG_DEBUG, "running in mode %i\n", c->mode);


	LIST(config);
	cw_cfg_set(&config, "timeout",         "5");
	cw_cfg_set(&config, "gatherers",       CW_GATHER_DIR "/*");
	cw_cfg_set(&config, "copydown",        CW_GATHER_DIR);
	cw_cfg_set(&config, "interval",        "300");
	cw_cfg_set(&config, "acl",             "/etc/clockwork/local.acl");
	cw_cfg_set(&config, "acl.default",     "deny");
	cw_cfg_set(&config, "syslog.ident",    "cogd");
	cw_cfg_set(&config, "syslog.facility", "daemon");
	cw_cfg_set(&config, "syslog.level",    "error");
	cw_cfg_set(&config, "security.cert",   "/etc/clockwork/certs/cogd");
	cw_cfg_set(&config, "pidfile",         "/var/run/cogd.pid");
	cw_cfg_set(&config, "lockdir",         "/var/lock/cogd");
	cw_cfg_set(&config, "difftool",        "/usr/bin/diff -u");

	cw_log_open(cw_cfg_get(&config, "syslog.ident"), "stderr");
	cw_log_level(0, (getenv("COGD_DEBUG") ? "debug" : "error"));
	cw_log(LOG_DEBUG, "default configuration:");
	cw_log(LOG_DEBUG, "  timeout         %s", cw_cfg_get(&config, "timeout"));
	cw_log(LOG_DEBUG, "  gatherers       %s", cw_cfg_get(&config, "gatherers"));
	cw_log(LOG_DEBUG, "  copydown        %s", cw_cfg_get(&config, "copydown"));
	cw_log(LOG_DEBUG, "  interval        %s", cw_cfg_get(&config, "interval"));
	cw_log(LOG_DEBUG, "  acl             %s", cw_cfg_get(&config, "acl"));
	cw_log(LOG_DEBUG, "  acl.default     %s", cw_cfg_get(&config, "acl.default"));
	cw_log(LOG_DEBUG, "  syslog.ident    %s", cw_cfg_get(&config, "syslog.ident"));
	cw_log(LOG_DEBUG, "  syslog.facility %s", cw_cfg_get(&config, "syslog.facility"));
	cw_log(LOG_DEBUG, "  syslog.level    %s", cw_cfg_get(&config, "syslog.level"));
	cw_log(LOG_DEBUG, "  security.cert   %s", cw_cfg_get(&config, "security.cert"));
	cw_log(LOG_DEBUG, "  pidfile         %s", cw_cfg_get(&config, "pidfile"));
	cw_log(LOG_DEBUG, "  lockdir         %s", cw_cfg_get(&config, "lockdir"));
	cw_log(LOG_DEBUG, "  difftool        %s", cw_cfg_get(&config, "difftool"));


	cw_log(LOG_DEBUG, "parsing cogd configuration file '%s'", config_file);
	FILE *io = fopen(config_file, "r");
	if (!io) {
		cw_log(LOG_WARNING, "Failed to read configuration from %s: %s",
			config_file, strerror(errno));
		cw_log(LOG_WARNING, "Using default configuration");

	} else {
		if (cw_cfg_read(&config, io) != 0) {
			cw_log(LOG_ERR, "Unable to parse %s");
			exit(1);
		}
		fclose(io);
		io = NULL;
	}


	cw_log(LOG_DEBUG, "determining adjusted log level/facility");
	int level = LOG_NOTICE;
	if (c->daemonize) {
		s = cw_cfg_get(&config, "syslog.level");
		cw_log(LOG_DEBUG, "configured log level is '%s', verbose modifier is %+i", s, verbose);
		level = cw_log_level_number(s);

		if (level < 0) {
			cw_log(LOG_WARNING, "'%s' is not a recognized log level, falling back to 'error'", s);
			level = LOG_ERR;
		}
	}
	level += verbose;
	if (level > LOG_DEBUG) level = LOG_DEBUG;
	if (level < 0) level = 0;
	cw_log(LOG_DEBUG, "adjusted log level is %s (%i)",
		cw_log_level_name(level), level);
	if (!c->daemonize) {
		cw_log(LOG_DEBUG, "Running in --foreground mode; forcing all logging to stderr");
		cw_cfg_set(&config, "syslog.facility", "stderr");
		umask(0);
	}
	if (c->mode == MODE_DUMP) {
		cw_log(LOG_DEBUG, "Running in --show-config mode; forcing all logging to stderr");
		cw_cfg_set(&config, "syslog.facility", "stderr");
	}
	cw_log(LOG_DEBUG, "redirecting to %s log as %s",
		cw_cfg_get(&config, "syslog.facility"),
		cw_cfg_get(&config, "syslog.ident"));

	cw_log_open(cw_cfg_get(&config, "syslog.ident"),
	            cw_cfg_get(&config, "syslog.facility"));
	cw_log_level(level, NULL);

	cw_log(LOG_DEBUG, "parsing master.* definitions into endpoint records");
	int n; char key[9] = "master.1", cert[7] = "cert.1";
	c->nmasters = 0;
	memset(c->masters, 0, sizeof(c->masters));
	for (n = 0; n < 8; n++, key[7]++, cert[5]++) {
		cw_log(LOG_DEBUG, "searching for %s", key);
		s = cw_cfg_get(&config, key);
		if (!s) break;
		cw_log(LOG_DEBUG, "found a master: %s", s);
		c->masters[n].endpoint = strdup(s);
		c->nmasters++;

		cw_log(LOG_DEBUG, "searching for %s", cert);
		s = cw_cfg_get(&config, cert);
		if (!s) break;
		cw_log(LOG_DEBUG, "found certificate: %s", s);
		c->masters[n].cert_file = strdup(s);
	}

	c->acl_file = strdup(cw_cfg_get(&config, "acl"));
	cw_log(LOG_DEBUG, "parsing stored ACLs from %s", c->acl_file);
	c->acl = cw_alloc(sizeof(cw_list_t)); cw_list_init(c->acl);
	acl_read(c->acl, c->acl_file);

	if (c->mode == MODE_DUMP) {
		int i;
		for (i = 0; i < c->nmasters; i++) {
			printf("master.%i        %s\n", i+1, c->masters[i].endpoint);
			if (c->masters[i].cert_file) {
				printf("cert.%i          %s\n", i+1, c->masters[i].cert_file);
			}
		}
		printf("timeout         %s\n", cw_cfg_get(&config, "timeout"));
		printf("gatherers       %s\n", cw_cfg_get(&config, "gatherers"));
		printf("copydown        %s\n", cw_cfg_get(&config, "copydown"));
		printf("interval        %s\n", cw_cfg_get(&config, "interval"));
		printf("acl             %s\n", cw_cfg_get(&config, "acl"));
		printf("acl.default     %s\n", cw_cfg_get(&config, "acl.default"));
		printf("syslog.ident    %s\n", cw_cfg_get(&config, "syslog.ident"));
		printf("syslog.facility %s\n", cw_cfg_get(&config, "syslog.facility"));
		printf("syslog.level    %s\n", cw_cfg_get(&config, "syslog.level"));
		printf("security.cert   %s\n", cw_cfg_get(&config, "security.cert"));
		printf("pidfile         %s\n", cw_cfg_get(&config, "pidfile"));
		printf("lockdir         %s\n", cw_cfg_get(&config, "lockdir"));
		printf("difftool        %s\n", cw_cfg_get(&config, "difftool"));
		exit(0);
	}


	if (c->nmasters == 0) {
		cw_log(LOG_ERR, "No masters defined in %s", config_file);
		exit(2);
	}

	int i;
	for (i = 0; i < c->nmasters; i++) {
		if (c->masters[i].cert_file) {
			cw_log(LOG_DEBUG, "Reading master.%i certificate from %s",
				i+1, c->masters[i].cert_file);
			if ((c->masters[i].cert = cw_cert_read(c->masters[i].cert_file)) != NULL)
				continue;
			cw_log(LOG_ERR, "cert.%i %s: %s", i+1, c->masters[i].cert_file,
				errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
			exit(2);
		} else {
			cw_log(LOG_ERR, "master.%i (%s) has no matching certificate (cert.%i)",
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

	s = cw_cfg_get(&config, "security.cert");
	cw_log(LOG_DEBUG, "reading public/private key from certificate file %s", s);
	c->cert = cw_cert_read(s);
	if (!c->cert) {
		cw_log(LOG_ERR, "%s: %s", s,
			errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
		exit(1);
	}
	if (!c->cert->seckey) {
		cw_log(LOG_ERR, "%s: No secret key found in certificate", s);
		exit(1);
	}
	if (!c->cert->ident) {
		cw_log(LOG_ERR, "%s: No identity found in certificate", s);
		exit(1);
	}


	cw_log(LOG_INFO, "cogd starting up");
	c->schedule.next_run = cw_time_ms();

	cw_log(LOG_DEBUG, "detecting fully-qualified domain name of local node");
	c->fqdn = cw_fqdn();
	if (!c->fqdn) exit(1);
	cw_log(LOG_INFO, "detected my FQDN as '%s'", c->fqdn);


	c->gatherers = strdup(cw_cfg_get(&config, "gatherers"));
	c->copydown  = strdup(cw_cfg_get(&config, "copydown"));
	c->difftool  = strdup(cw_cfg_get(&config, "difftool"));
	c->schedule.interval  = 1000 * atoi(cw_cfg_get(&config, "interval"));
	c->timeout            = 1000 * atoi(cw_cfg_get(&config, "timeout"));
	c->acl_default = strcmp(cw_cfg_get(&config, "acl.default"), "deny") == 0
		? ACL_DENY : ACL_ALLOW;

	unsetenv("COGD");
	if (c->daemonize) {
		if (setenv("COGD", "1", 1) != 0) {
			fprintf(stderr, "Failed to set COGD env var: %s\n", strerror(errno));
			exit(3);
		}
		cw_daemonize(cw_cfg_get(&config, "pidfile"), "root", "root");
	}


	c->zmq = zmq_ctx_new();
	c->zap = cw_zap_startup(c->zmq, NULL);

	if (c->mode == MODE_ONCE || c->mode == MODE_CODE) {
		cw_log(LOG_DEBUG, "Running under -X/--code or -1/--once; skipping bind");

	} else if (cw_cfg_get(&config, "mesh.broadcast")
	        && cw_cfg_get(&config, "mesh.control")
	        && cw_cfg_get(&config, "mesh.cert")) {

		cw_cert_t *ephemeral = cw_cert_generate(CW_CERT_TYPE_ENCRYPTION);
		cw_cert_t *mesh_cert = cw_cert_read(cw_cfg_get(&config, "mesh.cert"));
		if (!mesh_cert) {
			cw_log(LOG_ERR, "mesh.cert %s: %s", cw_cfg_get(&config, "mesh.cert"),
				errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
			exit(1);
		}
		if (!mesh_cert->pubkey) {
			cw_log(LOG_ERR, "No public key found in mesh.cert %s", cw_cfg_get(&config, "mesh.cert"));
			exit(1);
		}

		char *control   = cw_string("tcp://%s", cw_cfg_get(&config, "mesh.control"));
		char *broadcast = cw_string("tcp://%s", cw_cfg_get(&config, "mesh.broadcast"));

		int rc;
		cw_log(LOG_INFO, "Connecting to the mesh control at %s", control);
		c->control = zmq_socket(c->zmq, ZMQ_DEALER);
		rc = zmq_setsockopt(c->control, ZMQ_CURVE_SECRETKEY, cw_cert_secret(ephemeral), 32);
		assert(rc == 0);
		rc = zmq_setsockopt(c->control, ZMQ_CURVE_PUBLICKEY, cw_cert_public(ephemeral), 32);
		assert(rc == 0);
		rc = zmq_setsockopt(c->control, ZMQ_CURVE_SERVERKEY, cw_cert_public(mesh_cert), 32);
		assert(rc == 0);
		rc = zmq_connect(c->control, control);
		assert(rc == 0);


		cw_log(LOG_INFO, "Subscribing to the mesh broadcast at %s", broadcast);
		c->broadcast = zmq_socket(c->zmq, ZMQ_SUB);
		rc = zmq_setsockopt(c->broadcast, ZMQ_CURVE_SECRETKEY, cw_cert_secret(ephemeral), 32);
		assert(rc == 0);
		rc = zmq_setsockopt(c->broadcast, ZMQ_CURVE_PUBLICKEY, cw_cert_public(ephemeral), 32);
		assert(rc == 0);
		rc = zmq_setsockopt(c->broadcast, ZMQ_CURVE_SERVERKEY, cw_cert_public(mesh_cert), 32);
		assert(rc == 0);
		rc = zmq_connect(c->broadcast, broadcast);
		assert(rc == 0);
		rc = zmq_setsockopt(c->broadcast, ZMQ_SUBSCRIBE, NULL, 0);
		assert(rc == 0);


		cw_cert_destroy(ephemeral);

	} else {
		cw_log(LOG_INFO, "Skipping mesh registeration");
		c->broadcast = c->control = NULL;
	}

	c->cfm_lock = cw_string("%s/%s",
		cw_cfg_get(&config, "lockdir"),
		"cfm.lock");
	cw_log(LOG_DEBUG, "will use CFM lock file '%s'", c->cfm_lock);

	c->cfm_killswitch = cw_string("%s/%s",
		cw_cfg_get(&config, "lockdir"),
		"cfm.KILLSWITCH");
	cw_log(LOG_DEBUG, "will use CFM killswitch file '%s'", c->cfm_killswitch);

	cw_cfg_done(&config);
	return c;
}

static void s_client_free(client_t *c)
{
	assert(c);
	int i;
	cw_cert_destroy(c->cert);
	for (i = 0; i < c->nmasters; i++) {
		free(c->masters[i].endpoint);
		free(c->masters[i].cert_file);
		cw_cert_destroy(c->masters[i].cert);
	}

	acl_t *a, *a_tmp;
	for_each_object_safe(a, a_tmp, c->acl, l)
		acl_destroy(a);
	free(c->acl);
	free(c->acl_file);

	cw_hash_done(c->facts, 1);
	free(c->facts);

	free(c->gatherers);
	free(c->copydown);
	free(c->difftool);
	free(c->fqdn);
	free(c->cfm_lock);
	free(c->cfm_killswitch);

	if (c->broadcast) {
		cw_log(LOG_DEBUG, "shutting down mesh broadcast socket");
		cw_zmq_shutdown(c->broadcast, 0);
	}
	if (c->control) {
		cw_log(LOG_DEBUG, "shutting down mesh control socket");
		cw_zmq_shutdown(c->control, 0);
	}
	cw_zap_shutdown(c->zap);
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

	while (!cw_sig_interrupt()) {
		zmq_pollitem_t socks[] = {
			{ c->broadcast, 0, ZMQ_POLLIN, 0 },
		};

		int time_left = (int)((c->schedule.next_run - cw_time_ms()));
		if (time_left < 0) time_left = 0;
		cw_log(LOG_DEBUG, "zmq_poll for %ims", time_left);

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

			cw_pdu_t *pdu = cw_pdu_recv(c->broadcast);
			rc = mesh_client_handle(client, c->control, pdu);
			cw_pdu_destroy(pdu);

			mesh_client_destroy(client);
		}

		if (cw_time_ms() >= c->schedule.next_run) {
			s_cfm_run(c);
#ifdef UNIT_TESTS
			if (getenv("TEST_COGD_BAIL_EARLY"))
				exit(0);
#endif
		}
	}

shut_it_down:
	cw_log(LOG_INFO, "shutting down");

	s_client_free(c);
	cw_log_close();
	return 0;
}
