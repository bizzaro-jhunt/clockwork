#include <stdio.h>

#include "private.h"
#include "parser.h"

struct ast* parse_file(const char *path)
{
	spec_parser_context ctx;
	struct ast *root;

	ctx.file = NULL;
	ctx.files = stringlist_new(NULL);
	list_init(&ctx.fseen);

	yylex_init_extra(&ctx, &ctx.scanner);
	lexer_include_file(path, &ctx);
	yyparse(&ctx);

	root = ctx.root;

	yylex_destroy(ctx.scanner);
	stringlist_free(ctx.files);

	return root;
}

