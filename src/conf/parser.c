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
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "private.h"
#include "parser.h"

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
	free(ctx.file);
	fclose(ctx.io);

	if (ctx.errors > 0) {
		ERROR("Errors encountered; aborting...");
		return NULL;
	}

	return hash;
}

