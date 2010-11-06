%union {
	char *string;
	struct ast *node;
}

%token T_KEYWORD_POLICY
%token T_KEYWORD_IF
%token T_KEYWORD_UNLESS
%token T_KEYWORD_ELSE
%token T_KEYWORD_MAP
%token T_KEYWORD_IS

%token <string> T_IDENTIFIER
%token <string> T_FACT

%token <string> T_QSTRING
%token <string> T_NUMERIC

%type <node> policies       /* AST_OP_PROG */
%type <node> policy         /* AST_OP_POLICY */
%type <node> resources      /* AST_OP_PROG */
%type <node> resource       /* AST_OP_RES_* */
%type <node> attributes     /* AST_OP_PROG */
%type <node> attribute_spec /* AST_OP_SET_ATTRIBUTE */
%type <string> value

%{

#include <stdio.h>
#include <string.h>

#include "../ast.h"

/* defined in spec/lexer.l */
extern int yylex(void);
extern void yyerror(const char*);

static struct ast* parser_new_resource_node(const char *type);

static struct ast* parser_new_resource_node(const char *type)
{
	struct ast *node = ast_new(AST_OP_NOOP, NULL, NULL);
	if (!node) {
		return NULL;
	}

	if (strcmp(type, "user") == 0) {
		node->op = AST_OP_DEFINE_RES_USER;
	} else if (strcmp(type, "group") == 0) {
		node->op = AST_OP_DEFINE_RES_GROUP;
	} else if (strcmp(type, "file") == 0) {
		node->op = AST_OP_DEFINE_RES_FILE;
	} else {
		fprintf(stderr, "Unknown resource type: '%s'\n", type);
	}

	return node;
}

struct ast *spec_result = NULL;

%}

%%

policies:
	{ spec_result = ast_new(AST_OP_PROG, NULL, NULL); }
	| policies policy
	{ ast_add_child(spec_result, $2); }
	;

policy: T_KEYWORD_POLICY T_QSTRING '{' resources '}'
      { $$ = ast_new(AST_OP_DEFINE_POLICY, $2, NULL);
        ast_add_child($$, $4); }
      ;

resources:
	 { $$ = ast_new(AST_OP_PROG, NULL, NULL); }
	 | resources resource
	 { ast_add_child($$, $2); }
	 ;

resource: T_IDENTIFIER value '{' attributes '}'
	{ $$ = parser_new_resource_node($1);
	  ast_add_child($$, $4); }
	;

attributes:
	  { $$ = ast_new(AST_OP_PROG, NULL, NULL); }
	  | attributes attribute_spec
	  { ast_add_child($$, $2); }
	  ;

attribute_spec: T_IDENTIFIER ':' value
	      { $$ = ast_new(AST_OP_SET_ATTRIBUTE, $1, $3); }
	      ;

value: T_QSTRING
     | T_NUMERIC
     ;
