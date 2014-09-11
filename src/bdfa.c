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
#include <stdio.h>
#include <getopt.h>

#define MODE_EXTRACT 1
#define MODE_CREATE  2

int main(int argc, char **argv)
{
	int mode = 0;
	char *file = NULL;
	char *dir = NULL;

	const char *short_opts = "h?VcxC:f:";
	struct option long_opts[] = {
		{ "help",      no_argument,       NULL, 'h' },
		{ "version",   no_argument,       NULL, 'V' },
		{ "create",    no_argument,       NULL, 'c' },
		{ "extract",   no_argument,       NULL, 'x' },
		{ "directory", required_argument, NULL, 'C' },
		{ "file",      required_argument, NULL, 'f' },
		{ 0, 0, 0, 0 },
	};
	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			printf("bdfa, part of clockwork v%s\n", PACKAGE_VERSION);
			printf("Usage: bdfa [-h?xcV] [-f filename]\n\n");
			printf("Options:\n");
			printf("  -?, -h               show this help screen\n");
			printf("  -V, --version        show version information and exit\n");
			printf("  -c, --create         create a new BDFA archive\n");
			printf("  -x, --extract        extract files from a BDFA archive\n");
			printf("  -f filename          archive to read to / write from\n");
			printf("  -C directory         chdir here before create/extract\n");
			exit(0);
			break;

		case 'V':
			printf("bdfa (Clockwork) %s\n"
			       "Copyright (C) 2014 James Hunt\n",
			       PACKAGE_VERSION);
			exit(0);
			break;

		case 'c':
			mode = MODE_CREATE;
			break;

		case 'x':
			mode = MODE_EXTRACT;
			break;

		case 'f':
			file = optarg;
			break;

		case 'C':
			dir = optarg;
			break;
		}
	}

	if (mode == 0) {
		fprintf(stderr, "Missing -c or -x, I don't know what to do!\n");
		exit(1);
	}

	if (mode == MODE_CREATE && optind >= argc) {
		fprintf(stderr, "No paths given for new archive");
		exit(1);
	}

	FILE *io;
	if (mode == MODE_CREATE) {
		io = file ? fopen(file, "w") : stdout;
		if (!file) file = "stdout";
	} else {
		io = file ? fopen(file, "r") : stdin;
		if (!file) file = "stdin";
	}
	if (!io) {
		perror(file);
		exit(2);
	}

	if (dir) {
		if (chdir(dir) != 0) {
			perror(dir);
			exit(2);
		}
	}

	int rc = mode == MODE_CREATE
	       ? cw_bdfa_pack(fileno(io), argv[optind])
	       : cw_bdfa_unpack(fileno(io), argv[optind]);
	fclose(io);
	return rc;
}
