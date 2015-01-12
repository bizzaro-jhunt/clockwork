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
#include "mesh.h"
#include <getopt.h>

#define DEFAULT_CONFIG_FILE "/etc/clockwork/meshd.conf"

#define MODE_RUN  0
#define MODE_DUMP 1
#define MODE_TEST 2

static inline mesh_server_t *s_server_new(int argc, char **argv)
{
	char *t;
	int mode = MODE_RUN, daemon = 1;

	LIST(config);
	config_set(&config, "control",             "*:2315");
	config_set(&config, "broadcast",           "*:2316");
	config_set(&config, "client.connections",  "2048");
	config_set(&config, "client.expiration",   "600");
	config_set(&config, "syslog.ident",        "meshd");
	config_set(&config, "syslog.facility",     "daemon");
	config_set(&config, "syslog.level",        "error");
	config_set(&config, "auth.service",        "clockwork");
	config_set(&config, "auth.trusted",        "/etc/clockwork/auth/trusted");
	config_set(&config, "security.cert",       "/etc/clockwork/certs/meshd");
	config_set(&config, "acl.global",          "/etc/clockwork/global.acl");
	config_set(&config, "pidfile",             "/var/run/meshd.pid");

	log_open(config_get(&config, "syslog.ident"), "stderr");
	log_level(0, (getenv("MESHD_DEBUG") ? "debug" : "error"));
	logger(LOG_DEBUG, "default configuration:");
	logger(LOG_DEBUG, "  broadcast           %s", config_get(&config, "broadcast"));
	logger(LOG_DEBUG, "  control             %s", config_get(&config, "control"));
	logger(LOG_DEBUG, "  client.connections  %s", config_get(&config, "client.connections"));
	logger(LOG_DEBUG, "  client.expiration   %s", config_get(&config, "client.expiration"));
	logger(LOG_DEBUG, "  syslog.ident        %s", config_get(&config, "syslog.ident"));
	logger(LOG_DEBUG, "  syslog.facility     %s", config_get(&config, "syslog.facility"));
	logger(LOG_DEBUG, "  syslog.level        %s", config_get(&config, "syslog.level"));
	logger(LOG_DEBUG, "  auth.service        %s", config_get(&config, "auth.service"));
	logger(LOG_DEBUG, "  auth.trusted        %s", config_get(&config, "auth.trusted"));
	logger(LOG_DEBUG, "  security.cert       %s", config_get(&config, "security.cert"));
	logger(LOG_DEBUG, "  acl.global          %s", config_get(&config, "acl.global"));
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
			printf("meshd, part of clockwork v%s runtime %s\n",
				CLOCKWORK_VERSION, CLOCKWORK_RUNTIME);
			printf("Usage: meshd [-?hvqVFSt] [-c filename]\n\n");
			printf("Options:\n");
			printf("  -?, -h               show this help screen\n");
			printf("  -V, --version        show version information and exit\n");
			printf("  -v, --verbose        increase logging verbosity\n");
			printf("  -q, --quiet          disable logging\n");
			printf("  -F, --foreground     don't daemonize, run in the foreground\n");
			printf("  -S, --show-config    print configuration and exit\n");
			printf("  -t, --test           test configuration\n");
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
			printf("meshd (Clockwork) v%s runtime %s\n"
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
			daemon = 0;
			break;

		case 'S':
			logger(LOG_DEBUG, "handling -S/--show-config");
			mode = MODE_DUMP;
			break;

		case 't':
			logger(LOG_DEBUG, "handling -t/--test");
			mode = MODE_TEST;
			break;
		}
	}
	logger(LOG_DEBUG, "option processing complete");


	logger(LOG_DEBUG, "parsing meshd configuration file '%s'", config_file);
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
	if (!daemon) {
		logger(LOG_DEBUG, "Running in --foreground mode; forcing all logging to stdout");
		config_set(&config, "syslog.facility", "stdout");
	}
	if (mode == MODE_DUMP) {
		logger(LOG_DEBUG, "Running in --show-config mode; forcing all logging to stderr");
		config_set(&config, "syslog.facility", "stderr");
	}
	if (mode == MODE_TEST) {
		logger(LOG_DEBUG, "Running in --test mode; forcing all logging to stderr");
		config_set(&config, "syslog.facility", "stderr");
	}
	logger(LOG_DEBUG, "redirecting to %s log as %s",
		config_get(&config, "syslog.facility"),
		config_get(&config, "syslog.ident"));

	log_open(config_get(&config, "syslog.ident"),
	            config_get(&config, "syslog.facility"));
	log_level(level, NULL);


	logger(LOG_INFO, "meshd starting up");


	if (mode == MODE_DUMP) {
		printf("control             %s\n", config_get(&config, "control"));
		printf("broadcast           %s\n", config_get(&config, "broadcast"));
		printf("client.connections  %s\n", config_get(&config, "client.connections"));
		printf("client.expiration   %s\n", config_get(&config, "client.expiration"));
		printf("syslog.ident        %s\n", config_get(&config, "syslog.ident"));
		printf("syslog.facility     %s\n", config_get(&config, "syslog.facility"));
		printf("syslog.level        %s\n", config_get(&config, "syslog.level"));
		printf("auth.service        %s\n", config_get(&config, "auth.service"));
		printf("auth.trusted        %s\n", config_get(&config, "auth.trusted"));
		printf("security.cert       %s\n", config_get(&config, "security.cert"));
		printf("acl.global          %s\n", config_get(&config, "acl.global"));
		printf("pidfile             %s\n", config_get(&config, "pidfile"));
	}


	cert_t *cert = cert_read(config_get(&config, "security.cert"));
	if (!cert) {
		logger(LOG_ERR, "%s: %s", config_get(&config, "security.cert"),
			errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
		exit(1);
	}
	if (!cert->seckey) {
		logger(LOG_ERR, "%s: No secret key found in certificate",
			config_get(&config, "security.cert"));
		exit(1);
	}
	if (!cert->ident) {
		logger(LOG_ERR, "%s: No identity found in certificate",
			config_get(&config, "security.cert"));
		exit(1);
	}

	if (mode == MODE_TEST) {
		printf("Configuration OK\n");
		exit(0);
	}


	if (daemon)
		daemonize(config_get(&config, "pidfile"), "root", "root");

	mesh_server_t *s = mesh_server_new(NULL);
	int rc = mesh_server_setopt(s, MESH_SERVER_CERTIFICATE, cert, sizeof(cert_t));
	assert(rc == 0);
	cert_free(cert);

	size_t len = atoi(config_get(&config, "client.connections"));
	rc = mesh_server_setopt(s, MESH_SERVER_CACHE_SIZE, &len, sizeof(len));
	assert(rc == 0);

	int32_t life = atoi(config_get(&config, "client.expiration"));
	rc = mesh_server_setopt(s, MESH_SERVER_CACHE_LIFE, &life, sizeof(life));
	assert(rc == 0);

	t = config_get(&config, "auth.service");
	rc = mesh_server_setopt(s, MESH_SERVER_PAM_SERVICE, t, strlen(t));
	assert(rc == 0);

	t = config_get(&config, "auth.trusted");
	rc = mesh_server_setopt(s, MESH_SERVER_TRUSTDB, t, strlen(t));
	assert(rc == 0);

	t = config_get(&config, "acl.global");
	rc = mesh_server_setopt(s, MESH_SERVER_GLOBAL_ACL, t, strlen(t));
	assert(rc == 0);

	t = string("tcp://%s", config_get(&config, "control"));
	logger(LOG_DEBUG, "binding control channel %s", t);
	rc = mesh_server_bind_control(s, t);
	assert(rc == 0);
	free(t);

	t = string("tcp://%s", config_get(&config, "broadcast"));
	logger(LOG_DEBUG, "binding to broadcast channel %s", t);
	rc = mesh_server_bind_broadcast(s, t);
	assert(rc == 0);
	free(t);

	config_done(&config);
	logger(LOG_INFO, "meshd running");
	return s;
}

int main(int argc, char **argv)
{
#ifdef UNIT_TESTS
	/* only let unit tests run for 60s */
	alarm(60);
#endif
	mesh_server_t *s = s_server_new(argc, argv);
	mesh_server_run(s);
	mesh_server_destroy(s);

	logger(LOG_INFO, "shutting down");
	log_close();
	return 0;
}
