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

#include <libgen.h>
#include <stdio.h>
#include <glob.h>
#include <sys/stat.h>
#include <stdarg.h>

#include "../clockwork.h"

/** STATIC functions used only by other, non-static functions in this file **/

static FILE* lexer_open(const char *path, spec_parser_context *ctx)
{
	FILE *io;
	struct stat st;
	parser_file *seen;

	if (stat(path, &st) != 0) {
		/* NOTE: strerror not reentrant */
		spec_parser_error(ctx, "can't stat %s: %s", path, strerror(errno));
		return NULL;
	}

	if (!S_ISREG(st.st_mode)) {
		spec_parser_error(ctx, "can't open %s: not a regular file", path);
		return NULL;
	}

	/* Combination of st_dev and st_ino is guaranteed to be unique
	   on any system conforming to the POSIX specifications... */
	for_each_object(seen, &ctx->fseen, ls) {
		if (seen->st_dev == st.st_dev
		 && seen->st_ino == st.st_ino) {
			spec_parser_warning(ctx, "skipping %s (already seen)", path);
			return NULL;
		}
	}

	io = fopen(path, "r");
	if (!io) {
		spec_parser_error(ctx, "can't open %s: %s", path, strerror(errno));
		return NULL;
	}

	seen = malloc(sizeof(parser_file));
	if (!seen) {
		spec_parser_error(ctx, "out of memory");
		fclose(io);
		return NULL;
	}

	seen->st_dev = st.st_dev;
	seen->st_ino = st.st_ino;
	seen->io     = io;
	cw_list_init(&(seen->ls));
	cw_list_push(&ctx->fseen, &seen->ls);
	return io;
}

static int lexer_close(spec_parser_context *ctx)
{
	parser_file *seen;
	for_each_object_r(seen, &ctx->fseen, ls) {
		if (seen && seen->io) {
			fclose(seen->io);
			seen->io = NULL;
			return 0;
		}
	}
	return -1;
}

static void lexer_process_file(const char *path, spec_parser_context *ctx)
{
	FILE *io;
	void *buf;

	printf("INCLUDE:%s\n", path);

	io = lexer_open(path, ctx);
	if (!io) { /* already seen or some other error */
		return; /* bail; lexer_check_file already printed warnings */
	}

	buf = yy_create_buffer(io, YY_BUF_SIZE, ctx->scanner);
	yypush_buffer_state(buf, ctx->scanner);

	stringlist_add(ctx->files, path);
	ctx->file = ctx->files->strings[ctx->files->num-1];
}

/* Resolves a relative path spec (path) based on the parent
   directory of the current file being processed.  On the first
   run (when current_file is NULL) returns a copy of path,
   unmodified. */
static char* lexer_resolve_path_spec(const char *current_file, const char *path)
{
	char *file_dup, *base, *full_path;

	if (!current_file || path[0] == '/') {
		return strdup(path);
	}

	/* make a copy of current_file, since dirname(3)
	   _may_ modify its path argument. */
	file_dup = strdup(current_file);
	base = dirname(file_dup);

	full_path = cw_string("%s/%s", base, path);
	free(file_dup);

	return full_path;
}

void spec_parser_error(void *user, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	spec_parser_context *ctx = (spec_parser_context*)user;

	va_start(args, fmt);
	if (vsnprintf(buf, 256, fmt, args) < 0) {
		cw_log(LOG_CRIT, "%s:%u: error: vsnprintf failed in spec_parser_error",
		                ctx->file, yyget_lineno(ctx->scanner));
	} else {
		cw_log(LOG_CRIT, "%s:%u: error: %s", ctx->file, yyget_lineno(ctx->scanner), buf);
	}
	ctx->errors++;
}

void spec_parser_warning(void *user, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	spec_parser_context *ctx = (spec_parser_context*)user;

	va_start(args, fmt);
	if (vsnprintf(buf, 256, fmt, args) < 0) {
		cw_log(LOG_CRIT, "%s:%u: error: vsnprintf failed in spec_parser_warning",
		                ctx->file, yyget_lineno(ctx->scanner));
		ctx->errors++; /* treat this as an error */
	} else {
		cw_log(LOG_CRIT, "%s:%u: warning: %s", ctx->file, yyget_lineno(ctx->scanner), buf);
		ctx->warnings++; 
	}
}

void lexer_include_file(const char *path, spec_parser_context *ctx)
{
	glob_t expansion;
	size_t i;
	char *full_path;
	int rc;

	full_path = lexer_resolve_path_spec(ctx->file, path);
	if (!full_path) {
		spec_parser_error(ctx, "Unable to resolve absolute path of %s", path);
		return;
	}

	rc = glob(full_path, GLOB_MARK, NULL, &expansion);
	free(full_path);

	switch (rc) {
	case GLOB_NOMATCH:
		/* Treat path as a regular file path, not a glob */
		lexer_process_file(path, ctx);
		return;
	case GLOB_NOSPACE:
		spec_parser_error(ctx, "out of memory in glob expansion");
		return;
	case GLOB_ABORTED:
		spec_parser_error(ctx, "read aborted during glob expansion");
		return;
	}

	/* Process each the sorted list in gl_pathv, in reverse, so
	   that when buffers are popped off the stack, they come out
	   in alphabetical order.

	   n.b. size_t is unsigned; (size_t)(-1) > 0 */
	for (i = expansion.gl_pathc; i > 0; i--) {
		lexer_process_file(expansion.gl_pathv[i-1], ctx);
	}
	globfree(&expansion);
}

int lexer_include_return(spec_parser_context *ctx)
{
	printf("<<<<<<<: %s\n", ctx->file);
	stringlist_remove(ctx->files, ctx->file);
	lexer_close(ctx);

	if (ctx->files->num == 0) {
		return -1; /* no more files */
	}

	yypop_buffer_state(ctx->scanner);
	ctx->file = ctx->files->strings[ctx->files->num-1];
	return 0; /* keep going */
}

int yywrap(yyscan_t scanner) {
	/* lexer_include_return returns 0 if we need to keep going */
	printf("YYWRAP!\n");
	return lexer_include_return(yyget_extra(scanner));
}

