#include <stdio.h>
#include <string.h>

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
	lexer_new_buffer(io, &ctx);

fprintf(stderr, "Commence parsing\n");
	yyparse(&ctx);

fprintf(stderr, "Done parsing; grabbing root element.\n");
	root = ctx.root;
	yylex_destroy(ctx.scanner);

	return root;
}

