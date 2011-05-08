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
%x TEMPLATE
%x TPL_COMMENT
%%

\<%# { BEGIN(TPL_COMMENT); }

"<%" { BEGIN(TEMPLATE); }
"<%=" {
	BEGIN(TEMPLATE);
	return T_ECHO;
}

(.|\n) {
	yylval->singlec = yytext[0];
	return T_LITERAL;
}

<TPL_COMMENT>%> { BEGIN(INITIAL); }
<TPL_COMMENT>.  { /* Ignore comments */ }

<TEMPLATE>[ \t\r\n]* { /* Ignore whitespace */ }
<TEMPLATE>-%>\r?\n? { BEGIN(INITIAL); }
<TEMPLATE>%>        { BEGIN(INITIAL); }

<TEMPLATE>"if"     { return T_KEYWORD_IF; }
<TEMPLATE>"unless" { return T_KEYWORD_UNLESS; }
<TEMPLATE>"else"   { return T_KEYWORD_ELSE; }
<TEMPLATE>"is"     { return T_KEYWORD_IS; }
<TEMPLATE>"not"    { return T_KEYWORD_NOT; }
<TEMPLATE>"for"    { return T_KEYWORD_FOR; }
<TEMPLATE>"in"     { return T_KEYWORD_IN; }
<TEMPLATE>"end"    { return T_KEYWORD_END; }

<TEMPLATE>[\(\)] { return yytext[0]; }

<TEMPLATE>([a-z_]+\.)*[a-z_]+ { /* fact */
	yylval->string = strdup(yytext);
	return T_IDENTIFIER;
}
<TEMPLATE>[0-9]+ { /* numeric */
	yylval->string = strdup(yytext);
	return T_NUMERIC;
}
<TEMPLATE>\"[^\"\n]*[\"\n] { /* quoted string */
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
<TEMPLATE>. { template_parser_error(yyget_extra(yyscanner), "unexpected character '%c'", yytext[0]); }

%%
/* lexer_impl.c contains the implementation for routines used
   by the lexer.  They exist in a separate C file to keep this
   file clean and focused. */
#include "lexer_impl.c"
