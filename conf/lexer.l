%{
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

/**

  lexer.l - Reentrant Flex Lexical Analyzer

  This file defines the rules that generate a stream of tokens,
  to be sent to the Bison LALR parser.

 */
#include <stdio.h>

#include "private.h"
#include "grammar.h"

/* The following two macros are a convenience only, and CANNOT
   be used outside of a token action rule. */
#define lexer_error(s) conf_parser_error(yyget_extra(yyscanner), s);
#define lexer_warning(s) conf_parser_warning(yyget_extra(yyscanner), s);

/*
  The following flex options are necessary for proper operation
  of the generated lexer code:

    reentrant     - Generate a reentrant lexical analyzer
    bison-bridge  - We will be talking to a Bison parser
    yylineno      - Track line numbers internally (accessible
                    through the yylineno macro)
    noyywrap      - Do not attempt to wrap to another input buffer

  These options are defined to quell GCC compiler warnings related
  to defined but unused functions:

    noinput       - Do not define input (not currently used)
    nounput       - Do not define yyunpuy (not currently used)
 */
%}
%option reentrant bison-bridge
%option yylineno noyywrap
%option noinput nounput

%option prefix="yyconf"
%%

#.* { /* ignore comments */ }
[\t \r\n]+ { /* ignore whitespace */ }

[0-9]+ { /* numeric */
	yylval->string = strdup(yytext);
	return T_NUMERIC;
}
[a-zA-Z][a-zA-Z0-9_]* { /* identifier */
	yylval->string = strdup(yytext);
	return T_IDENTIFIER;
}
"=" { return yytext[0]; }
\"[^\"\n]*[\"\n] { /* quoted string */
	yylval->string = strdup(yytext + 1);
	if (yylval->string[yyleng-2] != '\"') {
		lexer_warning("unterminated string literal");
		/* recoverable error */
		yylval->string[yyleng-2] = '\0';
		yyless(yyleng-1);
	} else {
		yylval->string[yyleng-2] = '\0';
	}
	return T_QSTRING;
}
. { conf_parser_error(yyget_extra(yyscanner), "unexpected character '%c'", yytext[0]); }

%%
/* lexer_impl.c contains the implementation for routines used
   by the lexer.  They exist in a separate C file to keep this
   file clean and focused. */
#include "lexer_impl.c"
