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

#include "../../policy.h"
#include "../../spec/parser.h"

/** presence - a test utility

  presence reads a manifest file and then prints out
  all of the resources that are present and absent.
  The first line of the output contains user resources
  that are marked 'present', and the second line contains
  those that are absent.
 */

void print_users(struct policy *pol)
{
	struct res_user *u;
	struct resource *r;
	size_t i;

	i = 0;
	printf("present");
	for_each_resource(r, pol) {
		if (r->type != RES_USER) { continue; }
		u = (struct res_user*)(r->resource);

		if (!ENFORCED(u, RES_FILE_ABSENT)) {
			printf(":%s", u->ru_name);
			i++;
		}
	}
	if (i == 0) { printf(":none"); }
	printf("\n");

	i = 0;
	printf("absent");
	for_each_resource(r, pol) {
		if (r->type != RES_USER) { continue; }
		u = (struct res_user*)(r->resource);

		if (ENFORCED(u, RES_FILE_ABSENT)) {
			printf(":%s", u->ru_name);
			i++;
		}
	}
	if (i == 0) { printf(":none"); }
	printf("\n");
}

int main(int argc, char **argv)
{
	struct manifest *manifest;
	struct policy *pol;

	if (argc != 2) {
		printf("improper invocation!\n");
		return 1;
	}

	manifest = parse_file(argv[1]);
	if (!manifest) {
		printf("unable to parse_file!\n");
		return 2;
	}

	pol = policy_generate(manifest->root->nodes[0], NULL);
	if (!pol) {
		printf("unable to policy_generate!\n");
		return 3;
	}

	print_users(pol);

	return 0;
}
