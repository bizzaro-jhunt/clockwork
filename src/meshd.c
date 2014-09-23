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
#include "userdb.h"
#include "mesh.h"
#include <getopt.h>

#define DEFAULT_CONFIG_FILE "/etc/clockwork/meshd.conf"

#define MODE_RUN  0
#define MODE_DUMP 1
#define MODE_TEST 2

static inline mesh_server_t *s_server_new(int argc, char **argv)
{
	char *t;
	int mode = MODE_RUN, daemonize = 1;

	LIST(config);
	cw_cfg_set(&config, "control",             "*:2315");
	cw_cfg_set(&config, "broadcast",           "*:2316");
	cw_cfg_set(&config, "client.connections",  "2048");
	cw_cfg_set(&config, "client.expiration",   "600");
	cw_cfg_set(&config, "syslog.ident",        "meshd");
	cw_cfg_set(&config, "syslog.facility",     "daemon");
	cw_cfg_set(&config, "syslog.level",        "error");
	cw_cfg_set(&config, "auth.service",        "clockwork");
	cw_cfg_set(&config, "auth.trusted",        "/etc/clockwork/auth/trusted");
	cw_cfg_set(&config, "security.cert",       "/etc/clockwork/certs/meshd");
	cw_cfg_set(&config, "acl.global",          "/etc/clockwork/global.acl");
	cw_cfg_set(&config, "pidfile",             "/var/run/meshd.pid");

	cw_log_open(cw_cfg_get(&config, "syslog.ident"), "stderr");
	cw_log_level(0, (getenv("MESHD_DEBUG") ? "debug" : "error"));
	cw_log(LOG_DEBUG, "default configuration:");
	cw_log(LOG_DEBUG, "  broadcast           %s", cw_cfg_get(&config, "broadcast"));
	cw_log(LOG_DEBUG, "  control             %s", cw_cfg_get(&config, "control"));
	cw_log(LOG_DEBUG, "  client.connections  %s", cw_cfg_get(&config, "client.connections"));
	cw_log(LOG_DEBUG, "  client.expiration   %s", cw_cfg_get(&config, "client.expiration"));
	cw_log(LOG_DEBUG, "  syslog.ident        %s", cw_cfg_get(&config, "syslog.ident"));
	cw_log(LOG_DEBUG, "  syslog.facility     %s", cw_cfg_get(&config, "syslog.facility"));
	cw_log(LOG_DEBUG, "  syslog.level        %s", cw_cfg_get(&config, "syslog.level"));
	cw_log(LOG_DEBUG, "  auth.service        %s", cw_cfg_get(&config, "auth.service"));
	cw_log(LOG_DEBUG, "  auth.trusted        %s", cw_cfg_get(&config, "auth.trusted"));
	cw_log(LOG_DEBUG, "  security.cert       %s", cw_cfg_get(&config, "security.cert"));
	cw_log(LOG_DEBUG, "  acl.global          %s", cw_cfg_get(&config, "acl.global"));
	cw_log(LOG_DEBUG, "  pidfile             %s", cw_cfg_get(&config, "pidfile"));


	cw_log(LOG_DEBUG, "processing command-line options");
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
			cw_log(LOG_DEBUG, "handling -h/-?/--help");
			printf("meshd, part of clockwork v%s runtime %i\n",
				PACKAGE_VERSION, PENDULUM_VERSION);
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
			cw_log(LOG_DEBUG, "handling -v/--verbose (modifier = %i)", verbose);
			break;

		case 'q':
			verbose = 0;
			cw_log(LOG_DEBUG, "handling -q/--quiet (modifier = %i)", verbose);
			break;

		case 'V':
			cw_log(LOG_DEBUG, "handling -V/--version");
			printf("meshd (Clockwork) %s runtime v%04x\n"
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
			daemonize = 0;
			break;

		case 'S':
			cw_log(LOG_DEBUG, "handling -S/--show-config");
			mode = MODE_DUMP;
			break;

		case 't':
			cw_log(LOG_DEBUG, "handling -t/--test");
			mode = MODE_TEST;
			break;
		}
	}
	cw_log(LOG_DEBUG, "option processing complete");


	cw_log(LOG_DEBUG, "parsing meshd configuration file '%s'", config_file);
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
	if (verbose < 0) verbose = 0;
	t = cw_cfg_get(&config, "syslog.level");
	cw_log(LOG_DEBUG, "configured log level is '%s', verbose modifier is %+i", t, verbose);
	int level = cw_log_level_number(t);
	if (level < 0) {
		cw_log(LOG_WARNING, "'%s' is not a recognized log level, falling back to 'error'", t);
		level = LOG_ERR;
	}
	level += verbose;
	cw_log(LOG_DEBUG, "adjusted log level is %s (%i)",
		cw_log_level_name(level), level);
	if (!daemonize) {
		cw_log(LOG_DEBUG, "Running in --foreground mode; forcing all logging to stdout");
		cw_cfg_set(&config, "syslog.facility", "stdout");
	}
	if (mode == MODE_DUMP) {
		cw_log(LOG_DEBUG, "Running in --show-config mode; forcing all logging to stderr");
		cw_cfg_set(&config, "syslog.facility", "stderr");
	}
	if (mode == MODE_TEST) {
		cw_log(LOG_DEBUG, "Running in --test mode; forcing all logging to stderr");
		cw_cfg_set(&config, "syslog.facility", "stderr");
	}
	cw_log(LOG_DEBUG, "redirecting to %s log as %s",
		cw_cfg_get(&config, "syslog.facility"),
		cw_cfg_get(&config, "syslog.ident"));

	cw_log_open(cw_cfg_get(&config, "syslog.ident"),
	            cw_cfg_get(&config, "syslog.facility"));
	cw_log_level(level, NULL);


	cw_log(LOG_INFO, "meshd starting up");


	if (mode == MODE_DUMP) {
		printf("control             %s\n", cw_cfg_get(&config, "control"));
		printf("broadcast           %s\n", cw_cfg_get(&config, "broadcast"));
		printf("client.connections  %s\n", cw_cfg_get(&config, "client.connections"));
		printf("client.expiration   %s\n", cw_cfg_get(&config, "client.expiration"));
		printf("syslog.ident        %s\n", cw_cfg_get(&config, "syslog.ident"));
		printf("syslog.facility     %s\n", cw_cfg_get(&config, "syslog.facility"));
		printf("syslog.level        %s\n", cw_cfg_get(&config, "syslog.level"));
		printf("auth.service        %s\n", cw_cfg_get(&config, "auth.service"));
		printf("auth.trusted        %s\n", cw_cfg_get(&config, "auth.trusted"));
		printf("security.cert       %s\n", cw_cfg_get(&config, "security.cert"));
		printf("acl.global          %s\n", cw_cfg_get(&config, "acl.global"));
		printf("pidfile             %s\n", cw_cfg_get(&config, "pidfile"));
	}


	cw_cert_t *cert = cw_cert_read(cw_cfg_get(&config, "security.cert"));
	if (!cert) {
		cw_log(LOG_ERR, "%s: %s", cw_cfg_get(&config, "security.cert"),
			errno == EINVAL ? "Invalid Clockwork certificate" : strerror(errno));
		exit(1);
	}
	if (!cert->seckey) {
		cw_log(LOG_ERR, "%s: No secret key found in certificate",
			cw_cfg_get(&config, "security.cert"));
		exit(1);
	}
	if (!cert->ident) {
		cw_log(LOG_ERR, "%s: No identity found in certificate",
			cw_cfg_get(&config, "security.cert"));
		exit(1);
	}

	if (mode == MODE_TEST) {
		printf("Configuration OK\n");
		exit(0);
	}


	if (daemonize)
		cw_daemonize(cw_cfg_get(&config, "pidfile"), "root", "root");

	mesh_server_t *s = mesh_server_new(NULL);
	int rc = mesh_server_setopt(s, MESH_SERVER_CERTIFICATE, cert, sizeof(cw_cert_t));
	assert(rc == 0);
	cw_cert_destroy(cert);

	size_t len = atoi(cw_cfg_get(&config, "client.connections"));
	rc = mesh_server_setopt(s, MESH_SERVER_CACHE_SIZE, &len, sizeof(len));
	assert(rc == 0);

	int32_t life = atoi(cw_cfg_get(&config, "client.expiration"));
	rc = mesh_server_setopt(s, MESH_SERVER_CACHE_LIFE, &life, sizeof(life));
	assert(rc == 0);

	t = cw_cfg_get(&config, "auth.service");
	rc = mesh_server_setopt(s, MESH_SERVER_PAM_SERVICE, t, strlen(t));
	assert(rc == 0);

	t = cw_cfg_get(&config, "auth.trusted");
	rc = mesh_server_setopt(s, MESH_SERVER_TRUSTDB, t, strlen(t));
	assert(rc == 0);

	t = cw_cfg_get(&config, "acl.global");
	rc = mesh_server_setopt(s, MESH_SERVER_GLOBAL_ACL, t, strlen(t));
	assert(rc == 0);

	t = cw_string("tcp://%s", cw_cfg_get(&config, "control"));
	cw_log(LOG_DEBUG, "binding control channel %s", t);
	rc = mesh_server_bind_control(s, t);
	assert(rc == 0);
	free(t);

	t = cw_string("tcp://%s", cw_cfg_get(&config, "broadcast"));
	cw_log(LOG_DEBUG, "binding to broadcast channel %s", t);
	rc = mesh_server_bind_broadcast(s, t);
	assert(rc == 0);
	free(t);

	cw_cfg_done(&config);
	cw_log(LOG_INFO, "meshd running");
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

	cw_log(LOG_INFO, "shutting down");
	cw_log_close();
	return 0;
}
