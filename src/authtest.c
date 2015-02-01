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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <security/pam_appl.h>
#include <getopt.h>
#include "clockwork.h"

int main(int argc, char **argv)
{
	int rc;
	char *service = strdup(CW_PAM_SERVICE);
	char *username = NULL;
	char *password = NULL;

	const char *short_opts = "h?u:p:s:";
	struct option long_opts[] = {
		{ "help",     no_argument,       NULL, 'h' },
		{ "username", required_argument, NULL, 'u' },
		{ "password", required_argument, NULL, 'p' },
		{ "service",  required_argument, NULL, 's' },
		{ 0, 0, 0, 0 },
	};
	int opt, idx = 0;
	while ( (opt = getopt_long(argc, argv, short_opts, long_opts, &idx)) != -1) {
		switch (opt) {
		case 'h':
		case '?':
			printf("Usage: %s -u username -p password [-s clockwork]\n", argv[0]);
			exit(0);
			break;

		case 'u':
			free(username);
			username = strdup(optarg);
			break;

		case 'p':
			free(password);
			password = strdup(optarg);
			memset(optarg, 'x', strlen(optarg));
			break;

		case 's':
			free(service);
			service = strdup(optarg);
			break;
		}
	}

	if (!username) {
		printf("No username specified (see --username)\n");
		return 1;
	}
	if (!password) {
		printf("No password specified (see --password)\n");
		return 1;
	}

	rc = cw_authenticate(service, username, password);
	if (rc != 0) {
		fprintf(stderr, "%s@%s: %s\n", username, service, cw_autherror());
		return 1;
	} else {
		printf("%s@%s ok\n", username, service);
		return 0;
	}
}
