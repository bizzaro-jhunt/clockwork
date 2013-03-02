/*
  Copyright 2011-2013 James Hunt <james@jameshunt.us>

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

#ifndef _CONF_PRIVATE_H
#define _CONF_PRIVATE_H

/* This headers is "private" and includes structure definitions
   and function prototypes that are ONLY of interest to the
   internals of the lexer and grammar modules, conf/lexer.o
   and conf/grammar.o.
 */

#include <stdio.h>
#include "../clockwork.h"

/**
  conf_parser_context - User data for reentrant lexer / parser

  This structure bundles a bunch of variables that we need to keep
  track of while lexing and parsing.  An instance of this structure
  gets passed to yyconfparse, which propagates it through to yyconflex.

  The scanner member is an opaque pointer, allocated by yyconflex_init and
  freed by yyconflex_destroy.  It contains whatever Flex needs it to, and is
  used in place of global variables (like yyconflineno, yyconfleng, and friends).
 */
typedef struct {
	void *scanner;         /* lexer variable store; used instead of globals */

	unsigned int warnings; /* Number of times conf_parser_warning called */
	unsigned int errors;   /* Number of times conf_parser_error called */

	char *file;            /* Name of the current file being parsed */
	FILE *io;
	struct hash *config;
} conf_parser_context;

#define YY_EXTRA_TYPE conf_parser_context*
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

/*
  Set up parser and lexer parameters.

  YYPARSE_PARAM contains the name of the user data parameter
  of yyconfparse.  Bison will generate C code like this:

    void yyconfparse(void *YYPARSE_PARAM)

  This allow us to pass our conf_parser_context pointer around
  and keep track of the lexer scanner state.

  YYLEX_PARAM is defined as a way to extract the lexer state
  variable (the scanner member of conf_parser_context) so that
  it can be passed to yyconflex.  Flex doesn't care about the
  conf_parser_context structure at all.
 */
#define YYPARSE_PARAM ctx
#define YYLEX_PARAM   ((conf_parser_context*)ctx)->scanner

/* Defined in lexer.c */
int yyconflex(YYSTYPE*, yyscan_t);
int yyconflex_init_extra(YY_EXTRA_TYPE, yyscan_t*);
int yyconflex_destroy(yyscan_t);
void yyconfset_extra(YY_EXTRA_TYPE, yyscan_t);
int yyconfparse(void*);

/* Defined in lexer.l */
void conf_parser_error(void *ctx, const char *fmt, ...);
void conf_parser_warning(void *ctx, const char *fmt, ...);
/* Define yyconferror as a macro that invokes conf_parser_error

   Because the Flex-generated C code contains calls to
   yyconferror with the following signature:

     void yyconferror(const char*)

   This is the cleanest way to get error reporting with
   line numbers and other useful information.  It is
   worth pointing out that this definition will only work
   if yyconferror is called from yyconflex; otherwise the macro
   expansion of YYPARSE_PARAM is potentially invalid (there
   may not be a ctx variable to dereference).  Sine Flex
   only ever calls yyconferror from within yyconflex, this assumption
   is safe insofar as generated code is concerned.
 */
#define yyconferror(s) conf_parser_error(YYPARSE_PARAM, s);
int conf_parser_use_file(const char *path, conf_parser_context *ctx);

#endif
