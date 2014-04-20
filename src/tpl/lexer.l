%{
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
#define lexer_error(s) template_parser_error(yyget_extra(yyscanner), s);
#define lexer_warning(s) template_parser_warning(yyget_extra(yyscanner), s);

/*
  The following flex options are necessary for proper operation
  of the generated lexer code:

    reentrant     - Generate a reentrant lexical analyzer
    bison-bridge  - We will be talking to a Bison parser
    yylineno      - Track line numbers internally (accessible
                    through the yylineno macro)

  These options are defined to quell GCC compiler warnings related
  to defined but unused functions:

    noinput       - Do not define input (not currently used)
    nounput       - Do not define yyunpuy (not currently used)
 */
%}
%option reentrant bison-bridge
%option yylineno noyywrap
%option noinput nounput

%option prefix="yytpl"
%{
/*
  Define an exclusive start state for source text inside the template
  language delimiters.  The start delimiters (<% or <%=) put the lexer
  into the state, and the end delimiters (%> amd -%>) take it out.
 */
%}
%x TPL
%%

([^<%]+|<|%) {
	/* Kind of a silly way to match everything leading up to a
	   '<%.*' delimiter, but its the only way that works. */
	yylval->string = strdup(yytext);
	return T_LITERAL;
}

"<%"  { BEGIN(TPL); }
"<%=" { BEGIN(TPL); return T_ECHO; }
\<%#[^(%>)]+%>\r?\n? { /* Ignore comments */ }

<TPL>[ \t\r\n]* { /* Ignore whitespace */ }
<TPL>#.*        { /* Ignore comments   */ }

<TPL>-%>\r?\n? { BEGIN(INITIAL); }
<TPL>%>        { BEGIN(INITIAL); }

<TPL>"if"     { return T_KEYWORD_IF; }
<TPL>"unless" { return T_KEYWORD_UNLESS; }
<TPL>"else"   { return T_KEYWORD_ELSE; }
<TPL>"elsif"  { return T_KEYWORD_ELSIF; }
<TPL>"is"     { return T_KEYWORD_IS; }
<TPL>"not"    { return T_KEYWORD_NOT; }
<TPL>"end"    { return T_KEYWORD_END; }
<TPL>"="      { return T_OP_ASSIGN; }

<TPL>([a-z0-9_]+\.)+[a-z0-9_]+ { /* fact */
	yylval->string = strdup(yytext);
	return T_IDENTIFIER;
}
<TPL>[0-9]+ { /* numeric */
	yylval->string = strdup(yytext);
	return T_NUMERIC;
}
<TPL>\"[^\"\n]*[\"\n] { /* quoted string */
	yylval->string = strdup(yytext + 1);
	if (yylval->string[yyleng-2] != '\"') {
		lexer_warning("unterminated string literal");
		/* recoverable error */
		yyless(yyleng-1);
	} else {
		yylval->string[yyleng-2] = '\0';
	}
	return T_QSTRING;
}
<TPL>. { template_parser_error(yyget_extra(yyscanner), "unexpected character '%c'", yytext[0]); }

%%
/* lexer_impl.h contains the implementation for routines used
   by the lexer.  They exist in a separate C file to keep this
   file clean and focused. */
#include "lexer_impl.h"
