%{
#include <stdio.h>

#include "private.h"
#include "grammar.h"
%}
%option reentrant
%option bison-bridge
%option noyywrap noinput nounput
%option yylineno
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
#include "lexer_impl.c"
