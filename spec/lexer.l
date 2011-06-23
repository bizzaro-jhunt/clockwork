%{
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

"policy" { return T_KEYWORD_POLICY; }
"host"   { return T_KEYWORD_HOST; }
"if"     { return T_KEYWORD_IF; }
"unless" { return T_KEYWORD_UNLESS; }
"else"   { return T_KEYWORD_ELSE; }
"map"    { return T_KEYWORD_MAP; }
"is"     { return T_KEYWORD_IS; }
"not"    { return T_KEYWORD_NOT; }

depends[ \t\r\n]+on { return T_KEYWORD_DEPENDS_ON; }
affects             { return T_KEYWORD_AFFECTS; }

enforce { return T_KEYWORD_ENFORCE; }
extend  { return T_KEYWORD_EXTEND; }

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
	BEGIN(INITIAL);
}
<INCLUDE>. { spec_parser_error(yyget_extra(yyscanner), "unexpected character '%c'", yytext[0]); }

[{}\(\)\[\],:] { return yytext[0]; }

([a-z_][a-z_0-9]*\.)+[a-z_][a-z_0-9]* { /* fact */
	yylval->string = strdup(yytext);
	return T_FACT;
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
. { spec_parser_error(yyget_extra(yyscanner), "unexpected character '%c'", yytext[0]); }

%%
/* lexer_impl.c contains the implementation for routines used
   by the lexer.  They exist in a separate C file to keep this
   file clean and focused. */
#include "lexer_impl.c"
