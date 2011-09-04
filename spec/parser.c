/*
  Copyright 2011 James Hunt <james@jameshunt.us>

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
#include <sys/stat.h>

#include "private.h"
#include "parser.h"
#include "../policy.h"
#include "../log.h"
#include "../list.h"

static int _manifest_expand(struct manifest *manifest)
{
	unsigned int i;
	struct stree *pol, *node;

	for (i = 0; i < manifest->nodes_len; i++) {
		node = manifest->nodes[i];

		if (node->op == INCLUDE) {
			pol = hash_get(manifest->policies, node->data1);
			if (pol) {
				stree_add(node, pol);
				pol = NULL;
			} else {
				return -1;
			}
		}
	}

	return 0;
}

struct manifest* parse_file(const char *path)
{
	spec_parser_context ctx;
	struct manifest *manifest;
	struct stat init_stat;
	parser_file *seen, *tmp;

	/* check the file first. */
	if (stat(path, &init_stat) != 0) {
		return NULL;
	}

	ctx.file = NULL;
	ctx.warnings = ctx.errors = 0;
	ctx.files = stringlist_new(NULL);
	list_init(&ctx.fseen);

	yylex_init_extra(&ctx, &ctx.scanner);
	lexer_include_file(path, &ctx);
	yyparse(&ctx);

	manifest = ctx.root;

	yylex_destroy(ctx.scanner);
	stringlist_free(ctx.files);
	for_each_node_safe(seen, tmp, &ctx.fseen, ls) {
		free(seen);
	}

	if (ctx.errors > 0) {
		ERROR("Manifest parse errors encountered; aborting...");
		return NULL;
	}

	if (_manifest_expand(manifest) != 0) {
		manifest_free(manifest);
		return NULL;
	}

	return manifest;
}

