/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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

#include "vm.h"
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>

int main (int argc, char **argv)
{
	int trace = 0, debug = 0;
	int level = LOG_WARNING;
	const char *ident = "pn", *facility = "stdout";

	const char *short_opts = "h?vqDVTc";
	struct option long_opts[] = {
		{ "help",     no_argument,       NULL, 'h' },
		{ "verbose",  no_argument,       NULL, 'v' },
		{ "quiet",    no_argument,       NULL, 'q' },
		{ "debug",    no_argument,       NULL, 'D' },
		{ "version",  no_argument,       NULL, 'V' },
		{ "trace",    no_argument,       NULL, 'T' },
		{ "syslog",   required_argument, NULL,  0  },

		{ 0, 0, 0, 0 },
	};

	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			printf("USAGE: pn [OPTIONS] bin.b\n");
			exit(0);

		case 'v':
			level++;
			break;

		case 'q':
			level = 0;
			debug = 0;
			break;

		case 'D':
			debug = 1;
			break;

		case 'V':
			printf("pn (Clockwork) v%s runtime %s\n"
			       "Copyright (C) 2015 James Hunt\n",
			       CLOCKWORK_VERSION, CLOCKWORK_RUNTIME);
			exit(0);

		case 'T':
			trace = 1;
			break;

		case 0: /* --syslog */
			facility = argv[optind];
			break;
		}
	}

	if (optind != argc - 1) {
		printf("USAGE: pn [OPTIONS] bin.b\n");
		exit(0);
	}

	if (debug) level = LOG_DEBUG;
	log_open(ident, facility);
	log_level(level, NULL);

	int rc, fd = open(argv[optind], O_RDONLY);
	if (fd < 0) {
		perror(argv[optind]);
		return 1;
	}
	off_t n = lseek(fd, 0, SEEK_END);
	if (n < 0) {
		perror(argv[optind]);
		return 1;
	}
	if (n == 0) {
		fprintf(stderr, "%s: not a valid pendulum binary image\n", argv[optind]);
		return 1;
	}
	byte_t *code = mmap(NULL, n, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!code) {
		perror(argv[optind]);
		return 1;
	}

	vm_t vm;
	rc = vm_reset(&vm);
	assert(rc == 0);

	rc = vm_prime(&vm, code, n);
	assert(rc == 0);

	rc = vm_args(&vm, argc - optind, argv + optind);
	assert(rc == 0);

	vm.trace = trace;

	vm_exec(&vm);

	rc = vm_done(&vm);
	assert(rc == 0);

	return 0;
}
