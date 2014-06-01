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
#include <getopt.h>

#include "policy.h"

#define DEFAULT_CONFIG_FILE "/etc/clockwork/cogd.conf"

int main(int argc, char **argv)
{
	LIST(config);
	cw_cfg_set(&config, "listen",          "*:2304");
	cw_cfg_set(&config, "timeout",         "5");
	cw_cfg_set(&config, "gatherers",       CW_GATHER_DIR "/*");
	cw_cfg_set(&config, "copydown",        CW_GATHER_DIR);
	cw_cfg_set(&config, "interval",        "300");
	cw_cfg_set(&config, "syslog.ident",    "cogd");
	cw_cfg_set(&config, "syslog.facility", "daemon");
	cw_cfg_set(&config, "syslog.level",    "error");
	cw_cfg_set(&config, "pidfile",         "/var/run/cogd.pid");

	cw_log_open(cw_cfg_get(&config, "syslog.ident"), "stderr");
	cw_log_level(0, (getenv("COGD_DEBUG") ? "debug" : "error"));
	cw_log(LOG_DEBUG, "default configuration:");
	cw_log(LOG_DEBUG, "  listen          %s", cw_cfg_get(&config, "listen"));
	cw_log(LOG_DEBUG, "  timeout         %s", cw_cfg_get(&config, "timeout"));
	cw_log(LOG_DEBUG, "  gatherers       %s", cw_cfg_get(&config, "gatherers"));
	cw_log(LOG_DEBUG, "  copydown        %s", cw_cfg_get(&config, "copydown"));
	cw_log(LOG_DEBUG, "  interval        %s", cw_cfg_get(&config, "interval"));
	cw_log(LOG_DEBUG, "  syslog.ident    %s", cw_cfg_get(&config, "syslog.ident"));
	cw_log(LOG_DEBUG, "  syslog.facility %s", cw_cfg_get(&config, "syslog.facility"));
	cw_log(LOG_DEBUG, "  syslog.level    %s", cw_cfg_get(&config, "syslog.level"));
	cw_log(LOG_DEBUG, "  pidfile         %s", cw_cfg_get(&config, "pidfile"));


	int brief = 0;
	cw_log(LOG_DEBUG, "processing command-line options");
	const char *short_opts = "h?vqVc:b";
	struct option long_opts[] = {
		{ "help",        no_argument,       NULL, 'h' },
		{ "verbose",     no_argument,       NULL, 'v' },
		{ "quiet",       no_argument,       NULL, 'q' },
		{ "version",     no_argument,       NULL, 'V' },
		{ "config",      required_argument, NULL, 'c' },
		{ "brief",       no_argument,       NULL, 'b' },
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
			printf("facts, part of clockwork v%s\n", PACKAGE_VERSION);
			printf("Usage: fact [-?hvVqT] [-c filename] [fact [fact ...]]\n\n");
			printf("Options:\n");
			printf("  -?, -h               show this help screen\n");
			printf("  -V, --version        show version information and exit\n");
			printf("  -v, --verbose        increase logging verbosity\n");
			printf("  -q, --quiet          disable logging\n");
			printf("  -b, --brief          don't print fact names\n");
			printf("  -c filename          set configuration file (default: " DEFAULT_CONFIG_FILE ")\n");
			exit(0);
			break;

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
			printf("facts (Clockwork) %s\n"
			       "Copyright (C) 2014 James Hunt\n",
			       PACKAGE_VERSION);
			exit(0);
			break;

		case 'c':
			cw_log(LOG_DEBUG, "handling -c/--config; replacing '%s' with '%s'",
				config_file, optarg);
			config_file = optarg;
			break;

		case 'b':
			cw_log(LOG_DEBUG, "handling -b/--brief");
			brief = 1;
			break;
		}
	}
	cw_log(LOG_DEBUG, "option processing complete");


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
	if (verbose < 0) verbose = 0;
	char *s = cw_cfg_get(&config, "syslog.level");
	cw_log(LOG_DEBUG, "configured log level is '%s', verbose modifier is %+i", s, verbose);
	int level = cw_log_level_number(s);
	if (level < 0) {
		cw_log(LOG_WARNING, "'%s' is not a recognized log level, falling back to 'error'", s);
		level = LOG_ERR;
	}
	level += verbose;
	cw_log(LOG_DEBUG, "adjusted log level is %s (%i)",
		cw_log_level_name(level), level);
	cw_log(LOG_DEBUG, "Running in the foreground; forcing all logging to stderr");
	cw_cfg_set(&config, "syslog.facility", "stderr");

	cw_log(LOG_DEBUG, "redirecting to %s log as %s",
		cw_cfg_get(&config, "syslog.facility"),
		cw_cfg_get(&config, "syslog.ident"));

	cw_log_open(cw_cfg_get(&config, "syslog.ident"),
	            cw_cfg_get(&config, "syslog.facility"));
	cw_log_level(level, NULL);


	char *gatherers = cw_cfg_get(&config, "gatherers");
	cw_log(LOG_INFO, "gathering facts from %s", gatherers);

	cw_hash_t facts;
	memset(&facts, 0, sizeof(cw_hash_t));
	if (fact_gather(gatherers, &facts) != 0) {
		cw_log(LOG_CRIT, "Unable to gather facts from %s", gatherers);
		exit(1);
	}

	int rc = 0;
	if (optind >= argc) {
		fact_write(stdout, &facts);
	} else {
		char *v;
		for (; optind < argc; optind++) {
			v = cw_hash_get(&facts, argv[optind]);
			if (v) {
				if (brief)
					printf("%s\n", v);
				else
					printf("%s=%s\n", argv[optind], v);

			} else {
				fprintf(stderr, "No such fact '%s'\n", argv[optind]);
				rc = 2;
			}
		}

	}

	cw_log(LOG_INFO, "shutting down");
	return rc;
}
