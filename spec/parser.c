#include <stdio.h>

#include "private.h"
#include "parser.h"

struct ast* parse_file(const char *path)
{
	spec_parser_context ctx;
	FILE *io;
	struct ast *root;

	io = fopen(path, "r");
	if (!io) {
		return NULL;
	}

	yylex_init_extra(&ctx, &ctx.scanner);
	ctx.file = path; /* FIXME: should we strdup this? */
	lexer_new_buffer(io, &ctx);

	yyparse(&ctx);

	root = ctx.root;
	yylex_destroy(ctx.scanner);

	return root;
}

