#include <stdio.h>

#include "private.h"
#include "parser.h"

struct manifest* parse_file(const char *path)
{
	spec_parser_context ctx;
	struct manifest *root;

	ctx.file = NULL;
	ctx.warnings = ctx.errors = 0;
	ctx.files = stringlist_new(NULL);
	list_init(&ctx.fseen);

	yylex_init_extra(&ctx, &ctx.scanner);
	lexer_include_file(path, &ctx);
	yyparse(&ctx);

	root = ctx.root;

	yylex_destroy(ctx.scanner);
	stringlist_free(ctx.files);

	if (ctx.errors > 0) {
		fprintf(stderr, "Errors encountered; aborting...\n");
		return NULL;
	}

	stree_expand(root->root);
	return root;
}

