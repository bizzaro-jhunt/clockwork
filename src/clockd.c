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
#include <zmq.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <libgen.h>
#include <getopt.h>

#include "spec/parser.h"
#include "resources.h"
#include "pendulum.h"

#define BLOCK_SIZE 8192

#define DEFAULT_CONFIG_FILE "/etc/clockwork/clockd.conf"

typedef enum {
	STATE_INIT,
	STATE_IDENTIFIED,
	STATE_COPYDOWN,
	STATE_POLICY,
	STATE_FILE,
	STATE_REPORT,
} state_t;

static const char *FSM_STATES[] = {
	"INIT",
	"IDENTIFIED",
	"COPYDOWN",
	"POLICY",
	"FILE",
	"REPORT",
	NULL,
};

typedef enum {
	EVENT_UNKNOWN,
	EVENT_PING,
	EVENT_HELLO,
	EVENT_COPYDOWN,
	EVENT_POLICY,
	EVENT_FILE,
	EVENT_DATA,
	EVENT_REPORT,
	EVENT_BYE,
} event_t;

static const char *FSM_EVENTS[] = {
	"UNKNOWN",
	"PING",
	"HELLO",
	"COPYDOWN",
	"POLICY",
	"FILE",
	"DATA",
	"REPORT",
	"BYE",
	NULL,
};

static event_t s_pdu_event(pdu_t *pdu)
{
	int i;
	for (i = 0; FSM_EVENTS[i]; i++)
		if (strcmp(pdu_type(pdu), FSM_EVENTS[i]) == 0)
			return (event_t)i;
	return EVENT_UNKNOWN;
}

typedef enum {
	FSM_ERR_SUCCESS,
	FSM_ERR_INTERNAL,
	FSM_ERR_NOT_IMPLEMENTED,
	FSM_ERR_NO_POLICY_FOUND,
	FSM_ERR_NO_RESOURCE_FOUND,
	FSM_ERR_BADPROTO,
} fsm_err_t;

static const char *FSM_ERRORS[] = {
	"Success",
	"Internal Error",
	"Not Implemented",
	"Policy Not Found",
	"Resource Not Found",
	"Protocol Violation",
	NULL,
};

#define MODE_RUN  0
#define MODE_DUMP 1
#define MODE_TEST 2

typedef struct __client_t client_t;
typedef struct __server_t server_t;

struct __client_t {
	state_t           state;
	event_t           event;
	fsm_err_t         error;
	server_t         *server;

	char             *id;
	char             *name;
	struct stree     *pnode;
	struct policy    *policy;
	hash_t           *facts;

	FILE             *io;
	unsigned long     offset;
	struct SHA1       sha1;

	void             *mapped;
	size_t            maplen;
};

struct __server_t {
	void *zmq;
	void *listener;

	int mode;
	int daemonize;

	cache_t          *clients;
	struct manifest  *manifest;
	char             *copydown;

	cert_t     *cert;
	trustdb_t  *tdb;
	void       *zap;
};

static int s_sha1(client_t *fsm)
{
	struct sha1_ctx ctx;
	sha1_init(&fsm->sha1, NULL);
	sha1_ctx_init(&ctx);

	char data[BLOCK_SIZE];
	size_t n;
	int fd = fileno(fsm->io);
	while ((n = read(fd, data, BLOCK_SIZE)) > 0)
		sha1_ctx_update(&ctx, (uint8_t *)data, n);
	sha1_ctx_final(&ctx, fsm->sha1.raw);
	sha1_hexdigest(&fsm->sha1);

	rewind(fsm->io);
	return 0;
}

static char * s_gencode(client_t *c)
{
	FILE *io = tmpfile();
	assert(io);

	stopwatch_t t;
	uint32_t ms = 0;
	STOPWATCH(&t, ms) {
		policy_gencode(c->policy, io);
	}

	fprintf(io, "%c", '\0');
	c->maplen = ftell(io);
	c->mapped = mmap(NULL, c->maplen, PROT_READ, MAP_SHARED, fileno(io), 0);
	fclose(io);

	if (c->mapped == MAP_FAILED)
		c->mapped = NULL;

	float size;
	char unit = 'b';
	if (c->maplen > 1024 * 1024 * 1024) {
		size = c->maplen / 1024.0 / 1024.0 / 1024.0;
		unit = 'G';
	} else if (c->maplen > 1024 * 1024) {
		size = c->maplen / 1024.0 / 1024.0;
		unit = 'M';
	} else if (c->maplen > 1024) {
		size = c->maplen / 1024.0;
		unit = 'k';
	} else {
		size = c->maplen;
		unit = 'b';
	}
	logger(LOG_INFO, "generated %0.2f%c policy for %s in %lums", size, unit, c->name, ms);

	return (char *)c->mapped;
}

static int s_state_machine(client_t *fsm, pdu_t *pdu, pdu_t **reply)
{
	cache_touch(fsm->server->clients, fsm->id, 0);

	logger(LOG_DEBUG, "fsm: transition %s [%i] -> %s [%i]",
			FSM_STATES[fsm->state], fsm->state,
			FSM_EVENTS[fsm->event], fsm->event);

	switch (fsm->event) {
	case EVENT_UNKNOWN:
		fsm->error = FSM_ERR_BADPROTO;
		return 1;

	case EVENT_PING:
		*reply = pdu_reply(pdu, "PONG", 0);
		pdu_extendf(*reply, "%lu", CLOCKWORK_PROTOCOL);
		return 0;

	case EVENT_HELLO:
		switch (fsm->state) {
		case STATE_FILE:
		case STATE_COPYDOWN:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
			hash_done(fsm->facts, 1);
			fsm->facts = NULL;
			policy_free(fsm->policy);
			fsm->policy = NULL;

		case STATE_IDENTIFIED:
			free(fsm->name);

		case STATE_INIT:
		case STATE_REPORT:
			/* fall-through */
			break;
		}

		fsm->name = pdu_string(pdu, 1);
		*reply = pdu_reply(pdu, "OK", 0);

		fsm->state = STATE_IDENTIFIED;
		return 0;

	case EVENT_COPYDOWN:
		switch (fsm->state) {
		case STATE_INIT:
		case STATE_REPORT:
		case STATE_FILE:
		case STATE_COPYDOWN:
		case STATE_POLICY:
			fsm->error = FSM_ERR_BADPROTO;
			return 1;

		case STATE_IDENTIFIED:
			fsm->io = tmpfile();
			if (!fsm->io) {
				logger(LOG_ERR, "unable to create a temporary file for the copydown archive: %s",
					strerror(errno));
				fsm->error = FSM_ERR_INTERNAL;
				return 1;
			}

			if (cw_bdfa_pack(fileno(fsm->io), fsm->server->copydown) != 0) {
				logger(LOG_ERR, "unable to pack the copydown archive: %s",
					strerror(errno));
				fsm->error = FSM_ERR_INTERNAL;
				return 1;
			}
			rewind(fsm->io);
			fsm->offset = 0;

			*reply = pdu_reply(pdu, "OK", 0);
			break;
		}

		fsm->state = STATE_COPYDOWN;
		return 0;

	case EVENT_POLICY:
		switch (fsm->state) {
		case STATE_INIT:
			fsm->error = FSM_ERR_BADPROTO;
			return 1;

		case STATE_REPORT:
		case STATE_FILE:
		case STATE_COPYDOWN:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
			hash_done(fsm->facts, 1);
			fsm->facts = NULL;
			policy_free(fsm->policy);
			fsm->policy = NULL;

		case STATE_IDENTIFIED:
			/* fall-through */
			break;
		}

		char *facts = pdu_string(pdu, 2);
		fsm->facts = vmalloc(sizeof(hash_t));
		fact_read_string(facts, fsm->facts);
		free(facts);

		fsm->pnode = hash_get(fsm->server->manifest->hosts, fsm->name);
		if (!fsm->pnode) fsm->pnode = fsm->server->manifest->fallback;
		if (!fsm->pnode) {
			fsm->error = FSM_ERR_NO_POLICY_FOUND;
			return 1;
		}
		fsm->policy = policy_generate(fsm->pnode, fsm->facts);

		char *code = s_gencode(fsm);
		*reply = pdu_reply(pdu, "POLICY", 1, code);
		fsm->state = STATE_POLICY;
		return 0;

	case EVENT_FILE:
		switch (fsm->state) {
		case STATE_INIT:
		case STATE_IDENTIFIED:
		case STATE_COPYDOWN:
		case STATE_REPORT:
			fsm->error = FSM_ERR_BADPROTO;
			return 1;

		case STATE_FILE:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
			/* fall-through */
			break;
		}

		/* open the file, calculate the SHA1 */
		char *key = pdu_string(pdu, 1);
		struct resource *r = hash_get(fsm->policy->index, key);
		free(key);

		if (!r) {
			fsm->error = FSM_ERR_NO_RESOURCE_FOUND;
			return 1;
		}

		fsm->io = resource_content(r, fsm->facts);
		if (!fsm->io) {
			logger(LOG_ERR, "failed to generate content for %s (on behalf of %s): %s",
				r->key, fsm->name, strerror(errno));
			fsm->error = FSM_ERR_INTERNAL;
			return 1;
		}

		s_sha1(fsm);
		*reply = pdu_reply(pdu, "SHA1", 1, fsm->sha1.hex);
		fsm->state = STATE_FILE;
		return 0;

	case EVENT_DATA:
		switch (fsm->state) {
		case STATE_INIT:
		case STATE_IDENTIFIED:
		case STATE_POLICY:
		case STATE_REPORT:
			fsm->error = FSM_ERR_BADPROTO;
			return 1;

		case STATE_FILE:
		case STATE_COPYDOWN:
			/* fall-through */
			break;
		}

		char *off = pdu_string(pdu, 1);
		fsm->offset = BLOCK_SIZE * atoi(off);
		free(off);

		char block[BLOCK_SIZE+1] = "";
		fseek(fsm->io, fsm->offset, SEEK_SET);
		size_t n = fread(block, 1, BLOCK_SIZE, fsm->io);
		block[n] = '\0';

		if (n == 0) {
			if (feof(fsm->io)) {
				*reply = pdu_reply(pdu, "EOF", 0);
			} else {
				logger(LOG_ERR, "Failed to read from cached IO handled: %s", strerror(errno));
				*reply = pdu_reply(pdu, "ERROR", 1, "read error");
			}

		} else {
			*reply = pdu_reply(pdu, "BLOCK", 0);
			pdu_extend(*reply, block, n);
		}
		return 0;

	case EVENT_REPORT:
		switch (fsm->state) {
		case STATE_INIT:
		case STATE_IDENTIFIED:
			fsm->error = FSM_ERR_BADPROTO;
			return 1;

		case STATE_FILE:
		case STATE_COPYDOWN:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
		case STATE_REPORT:
			/* fall-through */
			break;
		}
		break;

	case EVENT_BYE:
		switch (fsm->state) {
		case STATE_REPORT:
		case STATE_FILE:
		case STATE_COPYDOWN:
			if (fsm->io) fclose(fsm->io);
			fsm->io = NULL;
			fsm->offset = 0;

		case STATE_POLICY:
			hash_done(fsm->facts, 1);
			free(fsm->facts);
			fsm->facts = NULL;
			policy_free_all(fsm->policy);
			fsm->policy = NULL;

		case STATE_IDENTIFIED:
			free(fsm->name);
			fsm->name = NULL;

		case STATE_INIT:
			/* fall-through */
			break;
		}

		fsm->state = STATE_INIT;
		cache_touch(fsm->server->clients, fsm->id, 1);
		*reply = pdu_reply(pdu, "BYE", 0);
		return 0;
	}

	fsm->error = FSM_ERR_INTERNAL;
	return 1;
}

static void s_client_destroy(void *p)
{
	if (!p) return;

	client_t *c = (client_t*)p;

	free(c->id);
	free(c->name);

	if (c->facts) {
		hash_done(c->facts, 1);
		free(c->facts);
	}

	if (c->io)
		fclose(c->io);

	policy_free_all(c->policy);
	free(c);
}

static inline server_t *s_server_new(int argc, char **argv)
{
	char *t;
	server_t *s = vmalloc(sizeof(server_t));
	s->daemonize = 1;

	CONFIG(config);
	config_set(&config, "listen",              "*:2314");
	config_set(&config, "ccache.connections",  "2048");
	config_set(&config, "ccache.expiration",   "600");
	config_set(&config, "manifest",            "/etc/clockwork/manifest.pol");
	config_set(&config, "copydown",            CW_GATHER_DIR);
	config_set(&config, "syslog.ident",        "clockd");
	config_set(&config, "syslog.facility",     "daemon");
	config_set(&config, "syslog.level",        "error");
	config_set(&config, "security.strict",     "yes");
	config_set(&config, "security.trusted",    "/etc/clockwork/certs/trusted");
	config_set(&config, "security.cert",       "/etc/clockwork/certs/clockd");
	config_set(&config, "pidfile",             "/var/run/clockd.pid");

	log_open(config_get(&config, "syslog.ident"), "stderr");
	log_level(0, (getenv("CLOCKD_DEBUG") ? "debug" : "error"));
	logger(LOG_DEBUG, "default configuration:");
	logger(LOG_DEBUG, "  listen              %s", config_get(&config, "listen"));
	logger(LOG_DEBUG, "  ccache.connections  %s", config_get(&config, "ccache.connections"));
	logger(LOG_DEBUG, "  ccache.expiration   %s", config_get(&config, "ccache.expiration"));
	logger(LOG_DEBUG, "  manifest            %s", config_get(&config, "manifest"));
	logger(LOG_DEBUG, "  copydown            %s", config_get(&config, "copydown"));
	logger(LOG_DEBUG, "  syslog.ident        %s", config_get(&config, "syslog.ident"));
	logger(LOG_DEBUG, "  syslog.facility     %s", config_get(&config, "syslog.facility"));
	logger(LOG_DEBUG, "  syslog.level        %s", config_get(&config, "syslog.level"));
	logger(LOG_DEBUG, "  security.strict     %s", config_get(&config, "security.strict"));
	logger(LOG_DEBUG, "  security.trusted    %s", config_get(&config, "security.trusted"));
	logger(LOG_DEBUG, "  security.cert       %s", config_get(&config, "security.cert"));
	logger(LOG_DEBUG, "  pidfile             %s", config_get(&config, "pidfile"));


	logger(LOG_DEBUG, "processing command-line options");
	const char *short_opts = "?hvqVFSt" "c:";
	struct option long_opts[] = {
		{ "help",        no_argument,       NULL, 'h' },
		{ "verbose",     no_argument,       NULL, 'v' },
		{ "quiet",       no_argument,       NULL, 'q' },
		{ "version",     no_argument,       NULL, 'V' },
		{ "foreground",  no_argument,       NULL, 'F' },
		{ "show-config", no_argument,       NULL, 'S' },
		{ "test",        no_argument,       NULL, 't' },
		{ "config",      required_argument, NULL, 'c' },
		{ 0, 0, 0, 0 },
	};
	int verbose = -1;
	const char *config_file = DEFAULT_CONFIG_FILE;
	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			logger(LOG_DEBUG, "handling -h/-?/--help");
			printf("clockd, part of clockwork v%s runtime %s protocol %i\n",
				CLOCKWORK_VERSION, CLOCKWORK_RUNTIME, CLOCKWORK_PROTOCOL);
			printf("Usage: clockd [-?hvqVFSt] [-c filename]\n\n");
			printf("Options:\n");
			printf("  -?, -h               show this help screen\n");
			printf("  -V, --version        show version information and exit\n");
			printf("  -v, --verbose        increase logging verbosity\n");
			printf("  -q, --quiet          disable logging\n");
			printf("  -F, --foreground     don't daemonize, run in the foreground\n");
			printf("  -S, --show-config    print configuration and exit\n");
			printf("  -t, --test           test configuration and manifest\n");
			printf("  -c filename          set configuration file (default: " DEFAULT_CONFIG_FILE ")\n");
			exit(0);

		case 'v':
			if (verbose < 0) verbose = 0;
			verbose++;
			logger(LOG_DEBUG, "handling -v/--verbose (modifier = %i)", verbose);
			break;

		case 'q':
			verbose = 0;
			logger(LOG_DEBUG, "handling -q/--quiet (modifier = %i)", verbose);
			break;

		case 'V':
			logger(LOG_DEBUG, "handling -V/--version");
			printf("clockd (Clockwork) v%s runtime %s\n"
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
			s->daemonize = 0;
			break;

		case 'S':
			logger(LOG_DEBUG, "handling -S/--show-config");
			s->mode = MODE_DUMP;
			break;

		case 't':
			logger(LOG_DEBUG, "handling -t/--test");
			s->mode = MODE_TEST;
			break;
		}
	}
	logger(LOG_DEBUG, "option processing complete");


	logger(LOG_DEBUG, "parsing clockd configuration file '%s'", config_file);
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
	if (verbose < 0) verbose = 0;
	t = config_get(&config, "syslog.level");
	logger(LOG_DEBUG, "configured log level is '%s', verbose modifier is %+i", t, verbose);
	int level = log_level_number(t);
	if (level < 0) {
		logger(LOG_WARNING, "'%s' is not a recognized log level, falling back to 'error'", t);
		level = LOG_ERR;
	}
	level += verbose;
	logger(LOG_DEBUG, "adjusted log level is %s (%i)",
		log_level_name(level), level);
	if (!s->daemonize) {
		logger(LOG_DEBUG, "Running in --foreground mode; forcing all logging to stdout");
		config_set(&config, "syslog.facility", "stdout");
	}
	if (s->mode == MODE_DUMP) {
		logger(LOG_DEBUG, "Running in --show-config mode; forcing all logging to stderr");
		config_set(&config, "syslog.facility", "stderr");
	}
	if (s->mode == MODE_TEST) {
		logger(LOG_DEBUG, "Running in --test mode; forcing all logging to stderr");
		config_set(&config, "syslog.facility", "stderr");
	}
	logger(LOG_DEBUG, "redirecting to %s log as %s",
		config_get(&config, "syslog.facility"),
		config_get(&config, "syslog.ident"));

	log_open(config_get(&config, "syslog.ident"),
	            config_get(&config, "syslog.facility"));
	log_level(level, NULL);

	logger(LOG_INFO, "clockd starting up");


	if (s->mode == MODE_DUMP) {
		printf("listen              %s\n", config_get(&config, "listen"));
		printf("ccache.connections  %s\n", config_get(&config, "ccache.connections"));
		printf("ccache.expiration   %s\n", config_get(&config, "ccache.expiration"));
		printf("manifest            %s\n", config_get(&config, "manifest"));
		printf("copydown            %s\n", config_get(&config, "copydown"));
		printf("syslog.ident        %s\n", config_get(&config, "syslog.ident"));
		printf("syslog.facility     %s\n", config_get(&config, "syslog.facility"));
		printf("syslog.level        %s\n", config_get(&config, "syslog.level"));
		printf("security.strict     %s\n", config_get(&config, "security.strict"));
		printf("security.trusted    %s\n", config_get(&config, "security.trusted"));
		printf("security.cert       %s\n", config_get(&config, "security.cert"));
		printf("pidfile             %s\n", config_get(&config, "pidfile"));
		exit(0);
	}


	s->cert = cert_read(config_get(&config, "security.cert"));
	if (!s->cert) {
		logger(LOG_ERR, "%s: %s", config_get(&config, "security.cert"),
			errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
		exit(1);
	}
	if (!s->cert->seckey) {
		logger(LOG_ERR, "%s: No secret key found in certificate",
			config_get(&config, "security.cert"));
		exit(1);
	}
	if (!s->cert->ident) {
		logger(LOG_ERR, "%s: No identity found in certificate",
			config_get(&config, "security.cert"));
		exit(1);
	}

	s->tdb = trustdb_read(config_get(&config, "security.trusted"));
	if (!s->tdb) s->tdb = trustdb_new();
	s->tdb->verify = strcmp(config_get(&config, "security.strict"), "no");
	logger(LOG_DEBUG, "%s certificate verification",
		s->tdb->verify ? "Enabling" : "Disabling");

	s->copydown = strdup(config_get(&config, "copydown"));
	s->manifest = parse_file(config_get(&config, "manifest"));
	if (!s->manifest) {
		if (errno)
			logger(LOG_CRIT, "Failed to parse %s: %s",
				config_get(&config, "manifest"), strerror(errno));
		exit(1);
	}
	if (s->mode == MODE_TEST) {
		if (manifest_validate(s->manifest) != 0)
			exit(2);
		printf("Syntax OK\n");
		exit(0);
	}

	if (s->daemonize)
		daemonize(config_get(&config, "pidfile"), "root", "root");
	s->clients = cache_new(atoi(config_get(&config, "ccache.connections")),
	                       atoi(config_get(&config, "ccache.expiration")));
	s->clients->destroy_f = s_client_destroy;


	s->zmq = zmq_ctx_new();
	s->zap = zap_startup(s->zmq, s->tdb);

	t = string("tcp://%s", config_get(&config, "listen"));
	logger(LOG_DEBUG, "binding to %s", t);
	s->listener = zmq_socket(s->zmq, ZMQ_ROUTER);

	int rc, optval = 1;
	logger(LOG_DEBUG, "Setting ZMQ_CURVE_SERVER to %i", optval);
	rc = zmq_setsockopt(s->listener, ZMQ_CURVE_SERVER, &optval, sizeof(optval));
	if (rc != 0) {
		logger(LOG_CRIT, "Failed to set ZMQ_CURVE_SERVER option on listening socket: %s",
			zmq_strerror(errno));
		if (errno == EINVAL) {
			int maj, min, rev;
			zmq_version(&maj, &min, &rev);
			logger(LOG_CRIT, "Perhaps the local libzmq (%u.%u.%u) doesn't support CURVE security?",
				maj, min, rev);
		}
		exit(4);
	}
	logger(LOG_DEBUG, "Setting ZMQ_CURVE_SECRETKEY (sec) to %s", s->cert->seckey_b16);
	rc = zmq_setsockopt(s->listener, ZMQ_CURVE_SECRETKEY, cert_secret(s->cert), 32);
	assert(rc == 0);

	rc = zmq_bind(s->listener, t);
	assert(rc == 0);
	free(t);

	config_done(&config);

	logger(LOG_INFO, "clockd running");
	return s;
}

static inline void s_server_destroy(server_t *s)
{
	cache_free(s->clients);
	manifest_free(s->manifest);
	cert_free(s->cert);
	trustdb_free(s->tdb);
	free(s->copydown);

	zap_shutdown(s->zap);
	zmq_ctx_destroy(s->zmq);

	free(s);
}

int main(int argc, char **argv)
{
#ifdef UNIT_TESTS
	/* only let unit tests run for 60s */
	alarm(60);
#endif
	server_t *s = s_server_new(argc, argv);
#ifdef UNIT_TESTS
	if (getenv("TEST_CLOCKD_BAIL_EARLY"))
		exit(0);
#endif
	signal_handlers();
	while (!signalled()) {
		logger(LOG_DEBUG, "awaiting inbound connection");
		pdu_t *pdu, *reply;
		pdu = pdu_recv(s->listener);
		if (!pdu) continue;

		logger(LOG_DEBUG, "received inbound connection, checking for client details");
		cache_purge(s->clients, 0);
		client_t *c = cache_get(s->clients, pdu_peer(pdu));
		if (!c) {
			c = vmalloc(sizeof(client_t));
			c->id = strdup(pdu_peer(pdu));

			if (!cache_set(s->clients, pdu_peer(pdu), c)) {
				logger(LOG_CRIT, "max connections reached!");
				reply = pdu_reply(pdu, "ERROR", 1, "Server busy; try again later\n");
				pdu_send_and_free(reply, s->listener);
				pdu_free(pdu);
				s_client_destroy(c);
				continue;
			}
		}
		logger(LOG_DEBUG, "inbound connection for client %p", c);

		c->server = s;
		c->event = s_pdu_event(pdu);
		int rc = s_state_machine(c, pdu, &reply);
		if (rc == 0) {
			logger(LOG_DEBUG, "%s: fsm is now at %s [%i]", pdu_peer(pdu), FSM_STATES[c->state], c->state);
			logger(LOG_DEBUG, "%s: sending back a %s PDU", pdu_peer(pdu), reply->type);
			pdu_send_and_free(reply, s->listener);
			pdu_free(pdu);

			if (c->mapped) {
				munmap(c->mapped, c->maplen);
				c->mapped = NULL;
				c->maplen = 0;
			}

#ifdef UNIT_TESTS
			if (c->event == EVENT_BYE)
				goto unit_tests_finished;
#endif

		} else {
			reply = pdu_reply(pdu, "ERROR", 1, FSM_ERRORS[c->error]);
			logger(LOG_DEBUG, "%s: sending back an ERROR PDU: %s", pdu_peer(pdu), FSM_ERRORS[c->error]);
			pdu_send_and_free(reply, s->listener);
			pdu_free(pdu);

#ifdef UNIT_TESTS
			goto unit_tests_finished;
#endif
		}
	}

#ifdef UNIT_TESTS
unit_tests_finished:
#endif
	logger(LOG_INFO, "shutting down");

	vzmq_shutdown(s->listener, 500);
	s_server_destroy(s);

	log_close();
	return 0;
}
