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

#include "../../clockwork.h"
#include "../../spec/parser.h"
#include "../../policy.h"

void print_policies(struct manifest *m)
{
	unsigned int i;
	struct stringlist *names;
	struct stree *root = m->root;

	names = stringlist_new(NULL);
	for (i = 0; i < root->size; i++) {
		stringlist_add(names, root->nodes[i]->data1);
	}

	printf("%s\n", stringlist_join(names, "::"));
}

int main(int argc, char **argv)
{
	struct manifest *manifest;

	if (argc != 2) {
		printf("improper invocation!\n");
		return 1;
	}
	manifest = parse_file(argv[1]);
	if (!manifest) {
		printf("unable to parse_file!\n");
		return 2;
	}

	print_policies(manifest);
	if (manifest->root->size == 0) {
		printf("no policies found!\n");
		return 3;
	}

	return 0;
}

