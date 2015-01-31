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
	const char *ident = "pn", *facility = "console";
	int mode = MODE_EXECUTE;
	int coverage = 0;
	int strip = 1;
	strings_t *inc = strings_new(NULL);

	const char *short_opts = "h?vqDVTSdCgI:";
	struct option long_opts[] = {
		{ "help",        no_argument,       NULL, 'h' },
		{ "verbose",     no_argument,       NULL, 'v' },
		{ "quiet",       no_argument,       NULL, 'q' },
		{ "debug",       no_argument,       NULL, 'D' },
		{ "version",     no_argument,       NULL, 'V' },
		{ "trace",       no_argument,       NULL, 'T' },
		{ "assemble",    no_argument,       NULL, 'S' },
		{ "disassemble", no_argument,       NULL, 'd' },
		{ "cover",       no_argument,       NULL, 'C' },
		{ "coverage",    no_argument,       NULL, 'C' },
		{ "annotations", no_argument,       NULL, 'g' },
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
			printf("pn (Clockwork) v%s runtime %i\n"
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

		case 'C':
			coverage = 1;
			break;

		case 'g':
			strip = 0;
			break;

		case 'I':
			strings_add(inc, optarg);
			break;

		case 0: /* --syslog */
			facility = optarg;
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
	logger(LOG_INFO, "pn starting up");

	if (mode == MODE_EXECUTE || mode == MODE_DISASSEMBLE) {
		logger(LOG_DEBUG, "running in execute / disassemble mode; a valid binary image is required");
		logger(LOG_DEBUG, "opening `%s' for execution / disassembly", argv[optind]);

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
			if (coverage) {
				char *cfile = string("%s.pcov", argv[optind]);
				FILE *io = fopen(cfile, "w");
				if (!io) {
					perror(cfile);
					free(cfile);
					return 1;
				}
				free(cfile);
				vm.ccovio = io;
			}

			rc = vm_args(&vm, argc - optind, argv + optind);
			assert(rc == 0);

			vm.trace = trace;
			vm_exec(&vm);

			rc = vm_done(&vm);
			assert(rc == 0);

			return 0;
		}

		if (mode == MODE_DISASSEMBLE) {
			vm_disasm(&vm, stdout);

			rc = vm_done(&vm);
			assert(rc == 0);

			return 0;
		}
	}

	if (mode == MODE_ASSEMBLE) {
		logger(LOG_DEBUG, "running in assembler mode");
		int rc, outfd = 1;

		asm_t *pna = asm_new();
		if (!pna) return 1;

		const char *path = argv[optind];
		if (strcmp(path, "-") == 0) {
			rc = asm_setopt(pna, PNASM_OPT_INIO, stdin, sizeof(stdin));
			if (rc != 0) goto bail;

			rc = asm_setopt(pna, PNASM_OPT_INFILE, "<stdin>", strlen("<stdin>"));
			if (rc != 0) goto bail;

		} else {
			rc = asm_setopt(pna, PNASM_OPT_INFILE, path, strlen(path));
			if (rc != 0) goto bail;
		}


		char *s = getenv("PENDULUM_INCLUDE");
		if (s)
			strings_add(inc, s);

		strings_add(inc, PENDULUM_INCLUDE);
		s = strings_join(inc, ":");
		logger(LOG_DEBUG, "setting include path to `%s'", s);
		rc = asm_setopt(pna, PNASM_OPT_INCLUDE, s, strlen(s));
		free(s);
		if (rc != 0) goto bail;

		rc = asm_setopt(pna, PNASM_OPT_STRIPPED, &strip, sizeof(strip));
		if (rc != 0) goto bail;

		rc = asm_compile(pna);
		if (rc != 0) goto bail;

		logger(LOG_DEBUG, "asm: assembly complete, writing bytecode image");
		if (strcmp(path, "-") == 0) {
			logger(LOG_DEBUG, "writing image to standard output");
			outfd = 1;

		} else {
			char *sfile = string("%s.S", argv[optind]);
			logger(LOG_DEBUG, "writing image to `%s'", sfile);

			outfd = open(sfile, O_WRONLY|O_CREAT|O_TRUNC, 0666);
			if (outfd < 0) {
				perror(sfile);
				return 1;
			}
			free(sfile);
		}

		if (write(outfd, pna->code, pna->size) != pna->size) {
			perror("short write");
			return 1;
		}
		return 0;

bail:
		logger(LOG_ERR, "assembly failed!");
		return 1;
	}

	return 3;
}
