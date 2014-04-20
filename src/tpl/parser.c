/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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
#include "../template.h"

struct template* parse_template(const char *path)
{
	template_parser_context ctx;
	struct template *template;
	struct stat init_stat;

	/* check the file first. */
	if (stat(path, &init_stat) != 0) {
		return NULL;
	}

	ctx.warnings = ctx.errors = 0;

	yytpllex_init_extra(&ctx, &ctx.scanner);
	template_parser_use_file(path, &ctx);

	template = template_new();
	ctx.root = template;
	yytplparse(&ctx);

	yytpllex_destroy(ctx.scanner);
	xfree(ctx.file);
	fclose(ctx.io);

	if (ctx.errors > 0) {
		ERROR("Template parse errors encountered; aborting...");
		return NULL;
	}

	return template;
}

