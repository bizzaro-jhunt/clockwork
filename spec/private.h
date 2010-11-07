#ifndef _SPEC_PRIVATE_H
#define _SPEC_PRIVATE_H

/* This headers is "private" and includes structure definitions
   and function prototypes that are ONLY of interest to the
   internals of the lexer and grammar modules, spec/lexer.o
   and spec/grammar.o.
 */

#include "../ast.h"
#include "../stringlist.h"

#ifndef YY_TYPEDEF_YY_BUFFER_STATE
#define YY_TYPEDEF_YY_BUFFER_STATE
typedef struct yy_buffer_state *YY_BUFFER_STATE;
#endif

typedef struct {
	const char     *fact;
	stringlist     *values;
	unsigned char   affirmative;
	struct ast     *then;
	struct ast     *otherwise;
} parser_branch;

typedef struct {
	const char     *fact;
	const char     *attribute;

	stringlist     *fact_values;
	stringlist     *attr_values;

	const char     *default_value;
} parser_map;

typedef struct {
	void *scanner;
	FILE *io;

	int success;
	struct ast *root;

	YY_BUFFER_STATE buf1;
} spec_parser_context;

#define YY_EXTRA_TYPE spec_parser_context*
typedef union {
	char           *string;
	stringlist     *strings;
	struct ast     *node;
	stringlist     *string_hash[2];
	char           *string_pair[2];

	parser_branch  *branch;
	parser_map     *map;
} YYSTYPE;
#define YYSTYPE_IS_DECLARED 1

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

#define YYPARSE_PARAM ctx
#define YYLEX_PARAM   ((spec_parser_context*)ctx)->scanner

int yylex(YYSTYPE*, yyscan_t);
int yylex_init_extra(YY_EXTRA_TYPE, yyscan_t*);
int yylex_destroy(yyscan_t);
void yyset_extra(YY_EXTRA_TYPE, yyscan_t);
int yyparse(void*);

/* Defined in lexer.l */
void parse_error(const char *error, void *user_data);
#define yyerror(s) parse_error(s, YYPARSE_PARAM)
int lexer_new_buffer(FILE*, spec_parser_context*);

#include "parser.h"

#endif
