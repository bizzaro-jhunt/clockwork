#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "private.h"
#include "parser.h"
#include "../log.h"

struct hash* parse_config(const char *path)
{
	config_parser_context ctx;
	struct hash *hash;

	ctx.file = NULL;
	ctx.warnings = ctx.errors = 0;

	yyconfiglex_init_extra(&ctx, &ctx.scanner);
	if (config_parser_use_file(path, &ctx) != 0) {
		return NULL;
	}
	yyconfigparse(&ctx);

	hash = ctx.config;
	yyconfiglex_destroy(ctx.scanner);

	if (ctx.errors > 0) {
		ERROR("Errors encountered; aborting...");
		return NULL;
	}

	return hash;
}

