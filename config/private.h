#ifndef _CONFIG_PRIVATE_H
#define _CONFIG_PRIVATE_H

/* This headers is "private" and includes structure definitions
   and function prototypes that are ONLY of interest to the
   internals of the lexer and grammar modules, spec/lexer.o
   and spec/grammar.o.
 */

#include "../hash.h"

/**
  config_parser_context - User data for reentrant lexer / parser

  This structure bundles a bunch of variables that we need to keep
  track of while lexing and parsing.  An instance of this structure
  gets passed to yyparse, which propagates it through to yylex.

  The scanner member is an opaque pointer, allocated by yylex_init and
  freed by yylex_destroy.  It contains whatever Flex needs it to, and is
  used in place of global variables (like yylineno, yyleng, and friends).
 */
typedef struct {
	void *scanner;         /* lexer variable store; used instead of globals */

	unsigned int warnings; /* Number of times config_parser_warning called */
	unsigned int errors;   /* Number of times config_parser_error called */

	const char *file;      /* Name of the current file being parsed */
	struct hash *config;
} config_parser_context;

#define YY_EXTRA_TYPE config_parser_context*
typedef union {
	char           *string;
	char           *pair[2];
	struct hash    *hash;
} YYSTYPE;
#define YYSTYPE_IS_DECLARED 1

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

/**
  Set up parser and lexer parameters.

  YYPARSE_PARAM contains the name of the user data parameter
  of yyparse.  Bison will generate C code like this:

    void yyparse(void *YYPARSE_PARAM)

  This allow us to pass our config_parser_context pointer around
  and keep track of the lexer scanner state.

  YYLEX_PARAM is defined as a way to extract the lexer state
  variable (the scanner member of config_parser_context) so that
  it can be passed to yylex.  Flex doesn't care about the
  config_parser_context structure at all.
 */
#define YYPARSE_PARAM ctx
#define YYLEX_PARAM   ((config_parser_context*)ctx)->scanner

/* Defined in lexer.c */
int yylex(YYSTYPE*, yyscan_t);
int yylex_init_extra(YY_EXTRA_TYPE, yyscan_t*);
int yylex_destroy(yyscan_t);
void yyset_extra(YY_EXTRA_TYPE, yyscan_t);
int yyparse(void*);

/* Defined in lexer.l */
void config_parser_error(void *ctx, const char *fmt, ...);
void config_parser_warning(void *ctx, const char *fmt, ...);
/* Define yyerror as a macro that invokes config_parser_error

   Because the Flex-generated C code contains calls to
   yyerror with the following signature:

     void yyerror(const char*)

   This is the cleanest way to get error reporting with
   line numbers and other useful information.  It is
   worth pointing out that this definition will only work
   if yyerror is called from yylex; otherwise the macro
   expansion of YYPARSE_PARAM is potentially invalid (there
   may not be a ctx variable to dereference).  Sine Flex
   only ever calls yyerror from within yylex, this assumption
   is safe insofar as generated code is concerned.
 */
#define yyerror(s) config_parser_error(YYPARSE_PARAM, s);
int config_parser_use_file(const char *path, config_parser_context *ctx);

#endif
