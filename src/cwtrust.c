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

#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "clockwork.h"

#define MODE_TRUST  1
#define MODE_REVOKE 2
#define MODE_LIST   3

int main(int argc, char **argv)
{
	const char *short_opts = "h?trd:";
	struct option long_opts[] = {
		{ "help",     no_argument,        NULL, 'h' },
		{ "trust",    no_argument,        NULL, 't' },
		{ "revoke",   no_argument,        NULL, 'r' },
		{ "database", required_argument,  NULL, 'd' },
		{ 0, 0, 0, 0 },
	};

	int mode = MODE_TRUST;
	char *path = strdup("/etc/clockwork/certs/trusted");

	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			printf("USAGE: %s [OPTIONS] FILE1 FILE2\n", argv[0]);
			exit(0);

		case 't':
			mode = MODE_TRUST;
			break;
		case 'r':
			mode = MODE_REVOKE;
			break;

		case 'd':
			free(path);
			path = strdup(optarg);
			break;
		}
	}

	cw_trustdb_t *db = cw_trustdb_read(path);
	if (!db) db = cw_trustdb_new();

	int rc;
	int i, n;
	for (i = optind, n = 0; i < argc; i++) {
		cw_cert_t *cert = cw_cert_read(argv[i]);
		if (!cert) {
			fprintf(stderr, "skipping %s: %s\n", argv[i], strerror(errno));
			continue;
		}

		if (!cert->pubkey) {
			fprintf(stderr, "skipping %s: no public key found in certificate\n", argv[i]);
			cw_cert_destroy(cert);
			continue;
		}

		if (mode == MODE_TRUST) {
			rc = cw_trustdb_trust(db, cert);
			assert(rc == 0);
			printf("TRUST %s %s\n", cert->pubkey_b16, cert->ident ? cert->ident : "(no ident)");
		} else {
			rc = cw_trustdb_revoke(db, cert);
			assert(rc == 0);
			printf("REVOKE %s %s\n", cert->pubkey_b16, cert->ident ? cert->ident : "(no ident)");
		}
		cw_cert_destroy(cert);
		n++;
	}

	printf("Processed %i certificate%s\n", n, n == 1 ? "" : "s");
	if (!n) exit(0);

	rc = cw_trustdb_write(db, path);
	if (rc != 0) {
		fprintf(stderr, "Failed to write out trustdb %s: %s\n", path, strerror(errno));
		exit(2);
	}

	printf("Wrote %s\n", path);
	free(path);
	cw_trustdb_destroy(db);
	return 0;
}
