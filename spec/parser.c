#include <stdio.h>
#include <sys/stat.h>

#include "private.h"
#include "parser.h"
#include "../policy.h"
#include "../log.h"

static int _manifest_expand(struct manifest *manifest)
{
	unsigned int i;
	struct stree *pol, *node;

	for (i = 0; i < manifest->nodes_len; i++) {
		node = manifest->nodes[i];

		if (node->op == INCLUDE) {
			pol = hash_get(manifest->policies, node->data1);
			if (pol) {
				node->op = PROG;
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

