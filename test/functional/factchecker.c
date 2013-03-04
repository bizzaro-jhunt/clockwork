/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

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
#include "../../resources.h"
#include "../../spec/parser.h"

/** factchecker - a test utility

  factchecker is responsible for reading facts from stdin,
  parsing a policy, and then evaluating that policy against
  the list of facts.

  The utility is very simple by design; it prints out all
  a list of usernames, uids and gids based on the res_user
  objects generated.  Only the first policy parsed is used.

  The intent of this utility is to allow the functional test
  suite to exercise the if and map constructs of the spec
  parser.
 */

void print_users(struct policy *pol)
{
	struct res_user *u;
	struct resource *r;
	size_t i = 0;

	for_each_resource(r, pol) {
		if (r->type != RES_USER) { continue; }
		u = (struct res_user*)(r->resource);
		printf("res_user:%s:%u:%u\n", u->ru_name, u->ru_uid, u->ru_gid);
		i++;
	}

	if (i == 0) {
		printf("no users defined!\n");
	}
}

int main(int argc, char **argv)
{
	struct hash *facts;
	struct manifest *manifest;
	struct policy *pol;

	if (argc != 2) {
		printf("improper invocation!\n");
		return 1;
	}

	facts = fact_read(stdin, NULL);
	manifest = parse_file(argv[1]);
	if (!manifest) {
		printf("unable to parse_file!\n");
		return 2;
	}

	pol = policy_generate(manifest->root->nodes[0], facts);
	if (!pol) {
		printf("unable to policy_generate!\n");
		return 3;
	}

	print_users(pol);

	return 0;
}
