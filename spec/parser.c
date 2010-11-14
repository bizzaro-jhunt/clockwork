#include <stdio.h>

#include "private.h"
#include "parser.h"

static void _manifest_expand(struct stree *root, struct hash *policies)
{
	unsigned int i;
	struct stree *pol;

	if (root->op == INCLUDE) {
		pol = hash_lookup(policies, root->data1);
		if (pol) {
			root->op = PROG;
			stree_add(root, pol->nodes[0]); /* FIXME: assumes that pol has nodes... */
		} else {
			/* FIXME: need to err */
		}
	}

	for (i = 0; i < root->size; i++) {
		_manifest_expand(root->nodes[i], policies);
	}
}


struct manifest* parse_file(const char *path)
{
	spec_parser_context ctx;
	struct manifest *manifest;

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
		fprintf(stderr, "Errors encountered; aborting...\n");
		return NULL;
	}

	_manifest_expand(manifest->root, manifest->policies);
	return manifest;
}

