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
	int trace = 0, check = 0, debug = 0, saferun = 0;
	int log_level = LOG_WARNING;
	const char *ident = "pn", *facility = "stdout";

	const char *short_opts = "h?vqDVTc";
	struct option long_opts[] = {
		{ "help",     no_argument,       NULL, 'h' },
		{ "verbose",  no_argument,       NULL, 'v' },
		{ "quiet",    no_argument,       NULL, 'q' },
		{ "debug",    no_argument,       NULL, 'D' },
		{ "version",  no_argument,       NULL, 'V' },
		{ "trace",    no_argument,       NULL, 'T' },
		{ "check",    no_argument,       NULL, 'c' },
		{ "syslog",   required_argument, NULL, '0' },
		{ "nofork",   no_argument,       NULL, '1' },

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
			debug = 0;
			break;

		case 'D':
			debug = 1;
			break;

		case 'V':
			printf("pn (Clockwork) %s runtime v%04x\n"
			       "Copyright (C) 2014 James Hunt\n",
			       PACKAGE_VERSION, PENDULUM_VERSION);
			exit(0);

		case 'T':
			trace = 1;
			break;

		case 'c':
			check = 1;
			break;

		case '0': /* --syslog */
			facility = argv[optind];
			break;

		case '1': /* --nofork */
			saferun = 0;
			break;
		}
	}

	if (optind != argc - 1) {
		printf("USAGE: pn [OPTIONS] script.pn\n");
		exit(0);
	}

	if (debug) log_level = LOG_DEBUG;
	cw_log_open(ident, facility);
	cw_log_level(log_level, NULL);

	FILE *io;
	if (strcmp(argv[optind], "-") == 0) {
		cw_log(LOG_INFO, "reading from standard input");
		io = tmpfile();
		if (!io) {
			cw_log(LOG_ERR, "failed to create temporary file: %s", strerror(errno));
			return 2;
		}
		char buf[8192];
		size_t nread, nwrit;
		for (;;) {
			nread = fread(buf, 1, 8192, stdin);
			cw_log(LOG_DEBUG, "read %li bytes from stdin", nread);
			if (nread <= 0) {
				if (feof(stdin)) break;
				cw_log(LOG_ERR, "read failed: %s", strerror(errno));
				fclose(io);
				return 2;
			}

			nwrit = fwrite(buf, 1, nread, io);
			cw_log(LOG_DEBUG, "wrote %li bytes to temporary file", nwrit);
			if (nwrit != nread) {
				cw_log(LOG_ERR, "failed to write to temp file: %s", strerror(errno));
				fclose(io);
				return 2;
			}
		}
	} else {
		cw_log(LOG_INFO, "reading from %s", argv[optind]);
		io = fopen(argv[optind], "r");
		if (!io) {
			fprintf(stderr, "Failed to open %s: %s\n",
					argv[optind], strerror(errno));
			return 2;
		}
	}

	pn_machine m;
	pn_init(&m);
	m.trace = trace;

	/* register clockwork pendulum functions */
	pendulum_init(&m, NULL);

	/* parse and run the input script */
	int rc = pn_parse(&m, io);
	if (rc != 0) {
		fprintf(stderr, "Syntax error: %s\n", strerror(errno));
		return 1;
	}
	if (check)
		printf("Syntax OK\n");
	else
		rc = saferun ? pn_run_safe(&m) : pn_run(&m);

	pendulum_destroy(&m);
	pn_destroy(&m);
	return rc;
}
