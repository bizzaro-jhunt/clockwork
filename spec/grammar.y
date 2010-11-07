%{
#include "parser.h"
%}

%union {
	char           *string;
	stringlist     *strings;
	struct ast     *node;
	stringlist     *string_hash[2];
	char           *string_pair[2];

	parser_branch  *branch;
	parser_map     *map;
}

%token T_KEYWORD_POLICY
%token T_KEYWORD_IF
%token T_KEYWORD_UNLESS
%token T_KEYWORD_ELSE
%token T_KEYWORD_MAP
%token T_KEYWORD_IS
%token T_KEYWORD_NOT

%token <string> T_IDENTIFIER
%token <string> T_FACT

%token <string> T_QSTRING
%token <string> T_NUMERIC

%type <node> policies            /* AST_OP_PROG */
%type <node> policy              /* AST_OP_POLICY */
%type <node> blocks              /* AST_OP_PROG */
%type <node> block               /* depends on type of block */
%type <node> conditional         /* AST_OP_IF_* */
%type <node> resource            /* AST_OP_RES_* */
%type <node> attributes          /* AST_OP_PROG */
%type <node> attribute_spec      /* AST_OP_SET_ATTRIBUTE */

%type <branch> conditional_test

%type <string>  value
%type <strings> value_list
%type <strings> explicit_value_list

%type <string> qstring

%type <map>         conditional_inline
%type <string_hash> mapped_value_set
%type <string_pair> mapped_value
%type <string>      mapped_value_default
%{

#include "parser.c"
struct ast *spec_result = NULL;

%}

%%

policies:
	{ spec_result = ast_new(AST_OP_PROG, NULL, NULL); }
	| policies policy
	{ ast_add_child(spec_result, $2); }
	;

policy: T_KEYWORD_POLICY qstring '{' blocks '}'
      { $$ = ast_new(AST_OP_DEFINE_POLICY, $2, NULL);
        ast_add_child($$, $4); }
      ;

blocks:
      { $$ = ast_new(AST_OP_PROG, NULL, NULL); }
      | blocks block
      { ast_add_child($$, $2); }
      ;

block: resource
     | conditional
     ;

resource: T_IDENTIFIER value '{' attributes '}'
	{ $$ = parser_new_resource_node($1, $2);
	  ast_add_child($$, $4); }
	;

attributes:
	  { $$ = ast_new(AST_OP_PROG, NULL, NULL); }
	  | attributes attribute_spec
	  { ast_add_child($$, $2); }
	  ;

attribute_spec: T_IDENTIFIER ':' value
	      { $$ = ast_new(AST_OP_SET_ATTRIBUTE, $1, $3); }
	      | T_IDENTIFIER ':' conditional_inline
	      { $3->attribute = $1;
	        $$ = map_expand_nodes($3); }
	      ;

value: qstring 
     | T_NUMERIC
     ;

conditional: T_KEYWORD_IF '(' conditional_test ')' '{' blocks '}'
	   { branch_connect($3, $6, ast_new(AST_OP_NOOP, NULL,  NULL));
	     $$ = branch_expand_nodes($3); }
	   | T_KEYWORD_IF '(' conditional_test ')' '{' blocks '}' T_KEYWORD_ELSE '{' blocks '}'
	   { branch_connect($3, $6, $10);
	     $$ = branch_expand_nodes($3); }
	   ;

conditional_test: T_FACT T_KEYWORD_IS value_list
		{ $$ = branch_new($1, $3, 1); }
		| T_FACT T_KEYWORD_IS T_KEYWORD_NOT value_list
		{ $$ = branch_new($1, $4, 0); }
		;

value_list : value
	   { $$ = stringlist_new(NULL);
	     stringlist_add($$, $1); }
	   | '[' explicit_value_list ']'
	   { $$ = $2; }
	   ;

explicit_value_list: value
		   { $$ = stringlist_new(NULL);
		     stringlist_add($$, $1); }
		   | explicit_value_list ',' value
		   { stringlist_add($$, $3); }
		   ;

conditional_inline: T_KEYWORD_MAP '(' T_FACT ')' '{' mapped_value_set mapped_value_default '}'
		  { $$ = map_new($3, NULL, $6[0], $6[1], $7); }
		  ;

mapped_value_set:
		{ $$[0] = stringlist_new(NULL);
		  $$[1] = stringlist_new(NULL); }
		| mapped_value_set mapped_value
		{ stringlist_add($$[0], $2[0]);
		  stringlist_add($$[1], $2[1]); }
		;

mapped_value: qstring ':' value
	    { $$[0] = $1;
	      $$[1] = $3; }
	    ;

mapped_value_default:
		    { $$ = NULL }
		    | T_KEYWORD_ELSE ':' value
		    { $$ = $3; }
		    ;

qstring: T_QSTRING
       | T_IDENTIFIER
       { yyerror("unexpected identifier; expected quoted string literal"); }
       ;
