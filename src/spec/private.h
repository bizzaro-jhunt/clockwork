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

#ifndef _SPEC_PRIVATE_H
#define _SPEC_PRIVATE_H

#include <sys/stat.h>

/* This headers is "private" and includes structure definitions
   and function prototypes that are ONLY of interest to the
   internals of the lexer and grammar modules, spec/lexer.o
   and spec/grammar.o.
 */

#include "../clockwork.h"
#include "../policy.h"

typedef struct {
	char         *attr;
	struct stree *lhs;
	cw_list_t     cond;
} parser_map;

typedef struct {
	cw_list_t l;
	struct stree *rhs;
	char         *value;
} parser_map_cond;

/**
  parser_file - Keep track of 'seen' files, by dev/inode

  Whenver the lexer encounters an 'include' macro, it needs
  to evaluate whether or not it has already seen that file,
  to avoid inclusion loops.  It also needs to keep track of
  the FILE* associated with the included file, so that it can
  be closed when the file has been completely read (otherwise,
  we leak the memory attached to the file handle).

  Device ID and Inode must be used in conjunction, in case an
  include macro reaches across filesystems / devices.
 */
typedef struct {
	dev_t           st_dev;        /* Device ID (filesystem) */
	ino_t           st_ino;        /* File Inode number */
	FILE           *io;            /* Open file handle */

	cw_list_t       ls;            /* For stacking parser_files */
} parser_file;

/**
  spec_parser_context - User data for reentrant lexer / parser

  This structure bundles a bunch of variables that we need to keep
  track of while lexing and parsing.  An instance of this structure
  gets passed to yyparse, which propagates it through to yylex.

  The scanner member is an opaque pointer, allocated by yylex_init and
  freed by yylex_destroy.  It contains whatever Flex needs it to, and is
  used in place of global variables (like yylineno, yyleng, and friends).

  The root member will be an abstract syntax tree node with an op of
  AST_OP_PROG (progression).  This node will contain syntax trees of all
  policy generators and host definitions found while parsing.
 */
typedef struct {
	void              *scanner;  /* lexer variable store; used instead of globals */

	unsigned int       warnings; /* Number of times spec_parser_warning called */
	unsigned int       errors;   /* Number of times spec_parser_error called */

	const char        *file;     /* Name of the current file being parsed */
	struct stringlist *files;    /* "Stack" of file names processed so far */
	cw_list_t          fseen;    /* List of device ID / inode pairs already include'd */

	struct manifest *root;
} spec_parser_context;

#define YY_EXTRA_TYPE spec_parser_context*
typedef union {
	char              *string;

	struct stree      *stree;
	struct manifest   *manifest;

	parser_map        *map;
	parser_map_cond   *map_cond;
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

    void yyparse(void *YYPARSE_PARAM)

  This allow us to pass our spec_parser_context pointer around
  and keep track of the lexer scanner state.

  YYLEX_PARAM is defined as a way to extract the lexer state
  variable (the scanner member of spec_parser_context) so that
  it can be passed to yylex.  Flex doesn't care about the
  spec_parser_context structure at all.
 */
#define YYPARSE_PARAM ctx
#define YYLEX_PARAM   ((spec_parser_context*)ctx)->scanner

/* Defined in lexer.c */
int yylex(YYSTYPE*, yyscan_t);
int yylex_init_extra(YY_EXTRA_TYPE, yyscan_t*);
int yylex_destroy(yyscan_t);
void yyset_extra(YY_EXTRA_TYPE, yyscan_t);
int yyparse(spec_parser_context*);

/* Defined in lexer.l */
void spec_parser_error(void *ctx, const char *fmt, ...);
void spec_parser_warning(void *ctx, const char *fmt, ...);
#define yyerror spec_parser_error

void lexer_include_file(const char *path, spec_parser_context*);
int lexer_include_return(spec_parser_context*);

#endif
