#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>
#include <getopt.h>

#include "clockwork.h"
#include "pendulum.h"
#include "pendulum_funcs.h"

int main(int argc, char **argv)
{
	int trace = 0;
	int log_level = LOG_INFO;
	const char *ident = "pn", *facility = "stdout";

	const char *short_opts = "h?vqVT";
	struct option long_opts[] = {
		{ "help",     no_argument,       NULL, 'h' },
		{ "verbose",  no_argument,       NULL, 'v' },
		{ "quiet",    no_argument,       NULL, 'q' },
		{ "version",  no_argument,       NULL, 'V' },
		{ "trace",    no_argument,       NULL, 'T' },

		{ "syslog",   required_argument, NULL, '0' },

		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			printf("USAGE: pn [OPTIONS] script.pn\n");
			exit(0);

		case 'v':
			log_level++;
			break;

		case 'q':
			log_level = 0;
			break;

		case 'V':
			printf("pn (Clockwork) %s runtime v%04x\n"
			       "Copyright (C) 2014 James Hunt\n",
			       PACKAGE_VERSION, PENDULUM_VERSION);
			exit(0);

		case 'T':
			trace = 1;
			break;

		case '0': /* --syslog */
			facility = argv[optind];
		}
	}

	if (optind != argc - 1) {
		printf("USAGE: pn [OPTIONS] script.pn\n");
		exit(0);
	}

	cw_log_open(ident, facility);
	cw_log_level(log_level, NULL);

	FILE *io = fopen(argv[optind], "r");
	if (!io) {
		fprintf(stderr, "Failed to open %s: %s\n",
				argv[optind], strerror(errno));
		return 2;
	}

	pn_machine m;
	pn_init(&m);
	m.trace = trace;

	/* register clockwork pendulum functions */
	pendulum_funcs(&m, NULL);

	/* parse and run the input script */
	pn_parse(&m, io);
	return pn_run_safe(&m);
}
