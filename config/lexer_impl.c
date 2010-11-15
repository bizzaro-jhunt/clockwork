#include <libgen.h>
#include <stdio.h>
#include <glob.h>
#include <sys/stat.h>
#include <stdarg.h>

/** STATIC functions used only by other, non-static functions in this file **/

void config_parser_error(void *user, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	config_parser_context *ctx = (config_parser_context*)user;

	va_start(args, fmt);
	if (vsnprintf(buf, 256, fmt, args) < 0) {
		fprintf(stderr, "%s:%u: error: vsnprintf failed in config_parser_error\n",
		                ctx->file, yyconfigget_lineno(ctx->scanner));
	} else {
		fprintf(stderr, "%s:%u: error: %s\n", ctx->file, yyconfigget_lineno(ctx->scanner), buf);
	}
	ctx->errors++;
}

void config_parser_warning(void *user, const char *fmt, ...)
{
	char buf[256];
	va_list args;
	config_parser_context *ctx = (config_parser_context*)user;

	va_start(args, fmt);
	if (vsnprintf(buf, 256, fmt, args) < 0) {
		fprintf(stderr, "%s:%u: error: vsnprintf failed in config_parser_warning\n",
		                ctx->file, yyconfigget_lineno(ctx->scanner));
		ctx->errors++; /* treat this as an error */
	} else {
		fprintf(stderr, "%s:%u: warning: %s\n", ctx->file, yyconfigget_lineno(ctx->scanner), buf);
		ctx->warnings++; 
	}
}

int config_parser_use_file(const char *path, config_parser_context *ctx)
{
	FILE *io;
	void *buf;

	io = fopen(path, "r");
	if (!io) {
		return -1;
	}

	buf = yyconfig_create_buffer(io, YY_BUF_SIZE, ctx->scanner);
	yyconfigpush_buffer_state(buf, ctx->scanner);
	ctx->file = strdup(path);

	return 0;
}

