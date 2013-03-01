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

#include <libgen.h>
#include <stdio.h>
#include <glob.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "../clockwork.h"

void template_parser_error(void *user, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	template_parser_context *ctx = (template_parser_context*)user;

	va_start(args, fmt);
	if (vsnprintf(buf, 256, fmt, args) < 0) {
		CRITICAL("%s:%u: error: vsnprintf failed in template_parser_error",
		                ctx->file, yytplget_lineno(ctx->scanner));
	} else {
		CRITICAL("%s:%u: error: %s", ctx->file, yytplget_lineno(ctx->scanner), buf);
	}
	ctx->errors++;
}

void template_parser_warning(void *user, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	template_parser_context *ctx = (template_parser_context*)user;

	va_start(args, fmt);
	if (vsnprintf(buf, 256, fmt, args) < 0) {
		CRITICAL("%s:%u: error: vsnprintf failed in template_parser_warning",
		                ctx->file, yytplget_lineno(ctx->scanner));
		ctx->errors++; /* treat this as an error */
	} else {
		CRITICAL("%s:%u: warning: %s", ctx->file, yytplget_lineno(ctx->scanner), buf);
		ctx->warnings++; 
	}
}

int template_parser_use_file(const char *path, template_parser_context *ctx)
{
	FILE *io;
	void *buf;

	io = fopen(path, "r");
	if (!io) {
		return -1;
	}

	buf = yytpl_create_buffer(io, YY_BUF_SIZE, ctx->scanner);
	yytplpush_buffer_state(buf, ctx->scanner);
	ctx->io   = io;
	ctx->file = strdup(path);

	return 0;
}
