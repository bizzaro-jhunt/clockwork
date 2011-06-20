#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "private.h"
#include "parser.h"
#include "../log.h"

struct hash* parse_config(const char *path)
{
	conf_parser_context ctx;
	struct hash *hash;

	ctx.file = NULL;
	ctx.warnings = ctx.errors = 0;

	yyconflex_init_extra(&ctx, &ctx.scanner);
	if (conf_parser_use_file(path, &ctx) != 0) {
		return NULL;
	}
	yyconfparse(&ctx);

	hash = ctx.config;
	yyconflex_destroy(ctx.scanner);

	if (ctx.errors > 0) {
		ERROR("Errors encountered; aborting...");
		return NULL;
	}

	return hash;
}

