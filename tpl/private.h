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

#ifndef _TPL_PRIVATE_H
#define _TPL_PRIVATE_H

#include <sys/stat.h>

/* This headers is "private" and includes structure definitions
   and function prototypes that are ONLY of interest to the
   internals of the lexer and grammar modules, tpl/lexer.o
   and tpl/grammar.o.
 */

#include "../clockwork.h"
#include "../template.h"

/**
  parser_branch - interim representation of an if block

  This structure is used internally by the Bison grammar to store
  information needed to construct an if-then-else form, prior to
  converting it into a string of syntax tree nodes.  This specific
  implementation is limited to 'if fact is [ v1, v2 ]' idioms.

  The 'affirmative' member is a boolean.  If it is set to 1, the
  if block contains an affirmative test; the 'then' block represents
  the control path to follow if the value of fact is contained in
  the values string list and the 'otherwise' block is the control path
  to follow if it is not.

  If affirmative is set to 0, the test is taken as a negation of
  'fact is [list]'.  The then and otherwise pointers have their
  meaning switched; if the value of fact is contained in the values
  string list, 'otherwise' is followed.  If not, 'then' is followed.
 */
typedef struct {
	const char     *fact;         /* Name of fact to test */
	struct stringlist *values;    /* List of values to check */
	unsigned char   affirmative;  /* see above */
	struct stree   *then;         /* The 'then' node, used in syntax tree conversion */
	struct stree   *otherwise;    /* The 'else' node, used in syntax tree conversion */
} parser_branch;

/**
  template_parser_context - User data for reentrant lexer / parser

  This structure bundles a bunch of variables that we need to keep
  track of while lexing and parsing.  An instance of this structure
  gets passed to yyparse, which propagates it through to yylex.

  The scanner member is an opaque pointer, allocated by yylex_init and
  freed by yylex_destroy.  It contains whatever Flex needs it to, and is
  used in place of global variables (like yylineno, yyleng, and friends).

  The root member will be an abstract syntax tree node with an op of
  TNODE_NOOP (progression).  This node will contain the full syntax tree.
 */
typedef struct {
	void *scanner;         /* lexer variable store; used instead of globals */

	unsigned int warnings; /* Number of times template_parser_warning called */
	unsigned int errors;   /* Number of times template_parser_error called */

	FILE *io;
	char *file;

	struct template *root;
} template_parser_context;

#define YY_EXTRA_TYPE template_parser_context*
typedef union {
	char            *string;
	char             singlec;
	struct string   *autostr;

	struct tnode    *tnode;
	struct template *tpl;
} YYSTYPE;
#define YYSTYPE_IS_DECLARED 1

#ifndef YY_TYPEDEF_YY_SCANNER_T
#define YY_TYPEDEF_YY_SCANNER_T
typedef void* yyscan_t;
#endif

/*
  Set up parser and lexer parameters.

  YYPARSE_PARAM contains the name of the user data parameter
  of yyparse.  Bison will generate C code like this:

    void yytplparse(void *YYPARSE_PARAM)

  This allow us to pass our template_parser_context pointer around
  and keep track of the lexer scanner state.

  YYLEX_PARAM is defined as a way to extract the lexer state
  variable (the scanner member of template_parser_context) so that
  it can be passed to yylex.  Flex doesn't care about the
  template_parser_context structure at all.
 */
#define YYPARSE_PARAM ctx
#define YYLEX_PARAM   ((template_parser_context*)ctx)->scanner

/* Defined in lexer.c */
int yytpllex(YYSTYPE*, yyscan_t);
int yytpllex_init_extra(YY_EXTRA_TYPE, yyscan_t*);
int yytpllex_destroy(yyscan_t);
void yytplset_extra(YY_EXTRA_TYPE, yyscan_t);
int yytplparse(void*);

/* Defined in lexer.l */
void template_parser_error(void *ctx, const char *fmt, ...);
void template_parser_warning(void *ctx, const char *fmt, ...);
/* Define yytplerror as a macro that invokes template_parser_error

   Because the Flex-generated C code contains calls to
   yytplerror with the following signature:

     void yytplerror(const char*)

   This is the cleanest way to get error reporting with
   line numbers and other useful information.  It is
   worth pointing out that this definition will only work
   if yytplerror is called from yylex; otherwise the macro
   expansion of YYPARSE_PARAM is potentially invalid (there
   may not be a ctx variable to dereference).  Sine Flex
   only ever calls yytplerror from within yylex, this assumption
   is safe insofar as generated code is concerned.
 */
#define yytplerror(s) template_parser_error(YYPARSE_PARAM, s);
int template_parser_use_file(const char *path, template_parser_context *ctx);

#endif
