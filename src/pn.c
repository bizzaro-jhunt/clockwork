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

#define MODE_EXECUTE     1
#define MODE_ASSEMBLE    2
#define MODE_DISASSEMBLE 3

int main (int argc, char **argv)
{
	int trace = 0, debug = 0;
	int level = LOG_WARNING;
	const char *ident = "pn", *facility = "stdout";
	int mode = MODE_EXECUTE;

	const char *short_opts = "h?vqDVTSd";
	struct option long_opts[] = {
		{ "help",        no_argument,       NULL, 'h' },
		{ "verbose",     no_argument,       NULL, 'v' },
		{ "quiet",       no_argument,       NULL, 'q' },
		{ "debug",       no_argument,       NULL, 'D' },
		{ "version",     no_argument,       NULL, 'V' },
		{ "trace",       no_argument,       NULL, 'T' },
		{ "assemble",    no_argument,       NULL, 'S' },
		{ "disassemble", no_argument,       NULL, 'd' },
		{ "syslog",      required_argument, NULL,  0  },

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

		case 'S':
			mode = MODE_ASSEMBLE;
			break;

		case 'd':
			mode = MODE_DISASSEMBLE;
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

	if (mode == MODE_EXECUTE || mode == MODE_DISASSEMBLE) {
		/* modes that require a valid binary image */
		off_t n = lseek(fd, 0, SEEK_END);
		if (n < 0) {
			perror(argv[optind]);
			return 1;
		}
		byte_t *code = mmap(NULL, n, PROT_READ, MAP_PRIVATE, fd, 0);
		if (!code) {
			perror(argv[optind]);
			return 1;
		}
		if (!vm_iscode(code, n)) {
			fprintf(stderr, "%s: not a Pendulum bytecode image.\n", argv[optind]);
			return 2;
		}

		vm_t vm;
		rc = vm_reset(&vm);
		assert(rc == 0);

		rc = vm_load(&vm, code, n);
		assert(rc == 0);

		if (mode == MODE_EXECUTE) {
			rc = vm_args(&vm, argc - optind, argv + optind);
			assert(rc == 0);

			vm.trace = trace;
			vm_exec(&vm);

			rc = vm_done(&vm);
			assert(rc == 0);

			return 0;
		}

		if (mode == MODE_DISASSEMBLE) {
			vm_disasm(&vm);

			rc = vm_done(&vm);
			assert(rc == 0);

			return 0;
		}
	}

	if (mode == MODE_ASSEMBLE) {
		byte_t *code;
		size_t len;

		FILE *io = fdopen(fd, "r");
		if (!io) {
			perror("fdopen");
			return 1;
		}

		rc = vm_asm_io(io, &code, &len);
		assert(rc == 0);

		char *sfile = string("%s.S", argv[optind]);
		int outfd = open(sfile, O_WRONLY|O_CREAT|O_EXCL, 0666);
		if (outfd < 0) {
			perror(sfile);
			free(sfile);
			return 1;
		}
		free(sfile);

		if (write(outfd, code, len) != len) {
			perror("short write");
			close(outfd);
			return 1;
		}
		close(outfd);
		return 0;
	}

	return 3;
}
