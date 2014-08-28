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
#define lexer_error(s) spec_parser_error(yyget_extra(yyscanner), s);
#define lexer_warning(s) spec_parser_warning(yyget_extra(yyscanner), s);

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
%option yylineno
%option noinput nounput
%{
/*
  Define an exclusive start state for 'include "file"' statements,
  so that the lexer can handle them, and the parser can remain blissfully
  unaware of what is going on.
 */
%}
%x INCLUDE
%%

#.* { /* ignore comments */ }
[\t \r\n]+ { /* ignore whitespace */ }

"allow"  { return T_KEYWORD_ALLOW; }
"deny"   { return T_KEYWORD_DENY;  }
"final"  { return T_KEYWORD_FINAL; }
"ALL"    { return T_KEYWORD_ALL;   }

"policy" { return T_KEYWORD_POLICY; }
"host"   { return T_KEYWORD_HOST; }
"if"     { return T_KEYWORD_IF; }
"unless" { return T_KEYWORD_UNLESS; }
"else"   { return T_KEYWORD_ELSE; }
"map"    { return T_KEYWORD_MAP; }
"default" { return T_KEYWORD_DEFAULT; }

"is"     { return T_KEYWORD_IS; }
"not"    { return T_KEYWORD_NOT; }
"like"   { return T_KEYWORD_LIKE; }

"!"      { return T_KEYWORD_NOT; }
"=="     { return T_KEYWORD_DOUBLE_EQUAL; }
"!="     { return T_KEYWORD_BANG_EQUAL; }
"=~"     { return T_KEYWORD_EQUAL_TILDE; }
"!~"     { return T_KEYWORD_BANG_TILDE; }

"or"     { return T_KEYWORD_OR; }
"||"     { return T_KEYWORD_OR; }
"and"    { return T_KEYWORD_AND; }
"&&"     { return T_KEYWORD_AND; }

depends[ \t\r\n]+on { return T_KEYWORD_DEPENDS_ON; }
affects             { return T_KEYWORD_AFFECTS; }

enforce { return T_KEYWORD_ENFORCE; }
extend  { return T_KEYWORD_EXTEND; }

defaults { return T_KEYWORD_DEFAULTS; }
fallback { return T_KEYWORD_FALLBACK; }

include[ \t]+  { BEGIN(INCLUDE); }
<INCLUDE>\"[^\"\n]*[\"\n] { 
	yylval->string = strdup(yytext + 1);
	if (yylval->string[yyleng-2] != '\"') {
		yylval->string[yyleng-2] = '\0';
		lexer_warning("unterminated string literal in include");
		/* recoverable error */
		yyless(yyleng-1);
	} else {
		yylval->string[yyleng-2] = '\0';
	}

	lexer_include_file(yylval->string, yyget_extra(yyscanner));
	free(yylval->string);
	BEGIN(INITIAL);
}
<INCLUDE>. { spec_parser_error(yyget_extra(yyscanner), "unexpected character '%c'", yytext[0]); }

[{}\(\)\[\],:] { return yytext[0]; }

([a-z_][a-z_0-9]*\.)+[a-z_][a-z_0-9]* { /* fact */
	yylval->string = strdup(yytext);
	return T_FACT;
}
%[a-zA-Z][a-zA-Z0-9_]+ { /* group identifier for ACLs */
	yylval->string = strdup(yytext+1);
	return T_ACLGROUP;
}
[a-z_]+ { /* identifier */
	yylval->string = strdup(yytext);
	return T_IDENTIFIER;
}
[0-9]+ { /* numeric */
	yylval->string = strdup(yytext);
	return T_NUMERIC;
}
\"[^\"\n]*[\"\n] { /* quoted string */
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
m?\/(\\\/|[^\/])*(\/i?|\n) { /* PCRE pattern */
	if (yytext[yyleng - 1] == '\n') {
		lexer_warning("unterminated regular expression");
		/* recoverable error */
		yyless(yyleng - 1);
	} else {
		yylval->string = strdup(yytext);
	}
	return T_REGEX;
}
m\|(\\\||[^\|])*(\|i?|\n) { /* PCRE pattern, m|...|, alternate delims */
	if (yytext[yyleng - 1] == '\n') {
		lexer_warning("unterminated regular expression");
		/* recoverable error */
		yyless(yyleng - 1);
	} else {
		yylval->string = strdup(yytext);
	}
	return T_REGEX;
}
. { spec_parser_error(yyget_extra(yyscanner), "unexpected character '%c'", yytext[0]); }

%%
/* lexer_impl.h contains the implementation for routines used
   by the lexer.  They exist in a separate C file to keep this
   file clean and focused. */
#include "lexer_impl.h"
