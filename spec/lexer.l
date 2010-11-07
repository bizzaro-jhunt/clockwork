%{
/**

  lexer.l - Reentrant Flex Lexical Analyzer

  This file defines the rules that generate a stream of tokens,
  to be sent to the Bison LALR parser.

 */
#include <stdio.h>

#include "private.h"
#include "grammar.h"

/*
  The following flex options are necessary for proper operation
  of the generated lexer code:

    reentrant     - Generate a reentrant lexical analyzer
    bison-bridge  - We will be talking to a Bison parser
    yylineno      - Track line numbers internally (accessible
                    through the yylineno macro)

  These options are defined to quell GCC compiler warnings related
  to defined but unused functions:

    noyywrap      - Do not define yywrap (not currently used)
    noinput       - Do not define input (not currently used)
    nounput       - Do not define yyunpuy (not currently used)
 */
%}
%option reentrant bison-bridge
%option yylineno
%option noyywrap noinput nounput
%%

#.* { /* ignore comments */ }
[\t \r\n]+ { /* ignore whitespace */ }

"policy" { return T_KEYWORD_POLICY; }
"if"     { return T_KEYWORD_IF; }
"unless" { return T_KEYWORD_UNLESS; }
"else"   { return T_KEYWORD_ELSE; }
"map"    { return T_KEYWORD_MAP; }
"is"     { return T_KEYWORD_IS; }
"not"    { return T_KEYWORD_NOT; }

[{}\(\)\[\],:] { return yytext[0]; }

([a-z]+\.)+[a-z]+ { /* fact */
	yylval->string = strdup(yytext);
	return T_FACT;
}
[a-z]+ { /* identifier */
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
		parse_error("unterminated string literal", yyget_extra(yyscanner));
		/* recoverable error */
		yyless(yyleng-1);
	} else {
		yylval->string[yyleng-2] = '\0';
	}
	return T_QSTRING;
}
. { parse_error("unexpected character", yyget_extra(yyscanner)); }

%%
/* lexer_impl.c contains the implementation for routines used
   by the lexer.  They exist in a separate C file to keep this
   file clean and focused. */
#include "lexer_impl.c"
