%{
/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

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

/**

  grammar.y - Reentrant (pure) Bison LALR Parser

  This file defines the productions necessary to interpret
  tokens found by the lexical analyzer, and subsquently build
  a hash table of configuration options

 */
#include "private.h"
%}

/*
  To get a reentrant Bison parser, we have to use the special
  '%pure-parser' directive.  Documentation on the 'net seems to
  disagree about whether this should be %pure-parser (with a hyphen)
  or %pure_parser (with an underscore).

  I have found %pure-parser to work just fine.  JRH */
%pure-parser

/* These token definitions identify the expected type of the lvalue.
   The name 'string' comes from the union members of the YYSTYPE
   union, defined in private.h

   N.B.: I deliberately do not use the %union construct provided by
   bison, opting to define the union myself in private.h.  If one of
   the possible lvalue types is not a basic type (like char*, int, etc.)
   then lexer is required to include the necessary header files. */
%token <string> T_IDENTIFIER
%token <string> T_QSTRING
%token <string> T_NUMERIC

/* Define the lvalue types of non-terminal productions.
   These definitions are necessary so that the $1..$n and $$ "magical"
   variables work in the generated C code. */
%type <pair>   directive
%type <string> value

%{
#define CONFIG(ctx) (((conf_parser_context*)ctx)->config)
%}
%%

configuration:
		{ CONFIG(ctx) = hash_new(); }
	| configuration directive
		{ hash_set(CONFIG(ctx), $2[0], $2[1]);
		  free($2[0]); }
	;

directive: T_IDENTIFIER '=' value
		{ $$[0] = $1;
		  $$[1] = $3; }
	;

value: T_QSTRING | T_IDENTIFIER | T_NUMERIC
	;
