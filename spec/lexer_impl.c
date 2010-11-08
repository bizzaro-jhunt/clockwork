#include <libgen.h>
#include <stdio.h>
#include <glob.h>
#include <sys/stat.h>

/** STATIC functions used only by other, non-static functions in this file **/

static int lexer_check_file(const char *path, spec_parser_context *ctx)
{
	struct stat st;
	parser_file *seen;

	if (stat(path, &st) != 0) {
		parse_error("Unable to stat a file %s: %s", ctx);
		return -1;
	}

	if (!S_ISREG(st.st_mode)) {
		parse_error("Unable to open file %s: %s", ctx);
		return -1;
	}

	/* Combination of st_dev and st_ino is guaranteed to be unique
	   on any system conforming to the POSIX specifications... */
	for_each_node(seen, &ctx->fseen, ls) {
		if (seen->st_dev == st.st_dev
		 && seen->st_ino == st.st_ino) {
			parse_error("Ignoring duplicate inclusion of %s", ctx);
			return -1;
		}
	}

	seen = malloc(sizeof(parser_file));
	if (!seen) {
		parse_error("out of memory", ctx);
		return -1;
	}

	seen->st_dev = st.st_dev;
	seen->st_ino = st.st_ino;
	list_add_tail(&seen->ls, &ctx->fseen);
	return 0;
}

static void lexer_process_file(const char *path, spec_parser_context *ctx)
{
	FILE *io;
	void *buf;

	if (lexer_check_file(path, ctx) != 0) { /* already seen */
		return; /* bail; lexer_check_file already printed warnings */
	}

	io = fopen(path, "r");
	if (!io) {
		parse_error("Unable to open file %s: %s", ctx);
		return;
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
	char *file_dup;
	char *base, *full_path;
	size_t len;

	if (!current_file || path[0] == '/') {
		return strdup(path);
	}

	/* make a copy of current_file, since dirname(3)
	   _may_ modify its path argument. */
	file_dup = strdup(current_file);
	base = dirname(file_dup);

	len = snprintf(NULL, 0, "%s/%s", base, path);
	if (len < 0) {
		return NULL;
	}

	full_path = malloc((len + 1) * sizeof(char));
	if (!full_path) {
		return NULL;
	}

	if (snprintf(full_path, len + 1, "%s/%s", base, path) < 0) {
		free(full_path);
		return NULL;
	}

	return full_path;
}

/* FIXME: make parse_error variadic, and break out into warning and error */
void parse_error(const char *s, void *user_data)
{
	spec_parser_context *ctx = (spec_parser_context*)user_data;

	fprintf(stderr, "%s:%u: %s\n", ctx->file, yyget_lineno(ctx->scanner), s);
}

void lexer_include_file(const char *path, spec_parser_context *ctx)
{
	glob_t expansion;
	size_t i;
	char *full_path;
	int rc;

	full_path = lexer_resolve_path_spec(ctx->file, path);
	if (!full_path) {
		parse_error("Unable to resolve absolute path of %s", ctx);
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
		parse_error("out of memory in glob expansion", ctx);
		return;
	case GLOB_ABORTED:
		parse_error("read aborted during glob expansion", ctx);
		return;
	}

	for (i = 0; i < expansion.gl_pathc; i++) {
		lexer_process_file(expansion.gl_pathv[i], ctx);
	}
	globfree(&expansion);
}

int lexer_include_return(spec_parser_context *ctx)
{
	stringlist_remove(ctx->files, ctx->file);
	if (ctx->files->num == 0) {
		return -1; /* no more files */
	}

	yypop_buffer_state(ctx->scanner);
	ctx->file = ctx->files->strings[ctx->files->num-1];
	return 0; /* keep going */
}

int yywrap(yyscan_t scanner) {
	/* lexer_include_return returns 0 if we need to keep going */
	return lexer_include_return(yyget_extra(scanner));
}

