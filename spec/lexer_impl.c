
void parse_error(const char *s, void *user_data)
{
	spec_parser_context *ctx = (spec_parser_context*)user_data;

	fprintf(stderr, "FILE:%u: %s\n", yyget_lineno(ctx->scanner), s);
}

int lexer_new_buffer(FILE *io, spec_parser_context *ctx)
{
	ctx->buf1 = yy_create_buffer(io, YY_BUF_SIZE, ctx->scanner);
	yy_switch_to_buffer(ctx->buf1, ctx->scanner);

	return 0; /* FIXME: hard-coded return value */
}
