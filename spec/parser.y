%{
#include <stdio.h>
#include <string.h>

#include "../ast.h"
#include "../stringlist.h"

struct branch {
	const char     *fact;
	stringlist     *values;
	unsigned char   affirmative;
	struct ast     *then;
	struct ast     *otherwise;
};

struct map {
	const char     *fact;
	const char     *attribute;

	stringlist     *fact_values;
	stringlist     *attr_values;

	const char     *default_value;
};

%}

%union {
	char           *string;
	stringlist     *strings;
	struct ast     *node;
	struct branch  *branch;
	struct map     *map;
	stringlist     *string_hash[2];
	char           *string_pair[2];
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

%type <map>         conditional_inline
%type <string_hash> mapped_value_set
%type <string_pair> mapped_value
%type <string>      mapped_value_default
%{

/* defined in spec/lexer.l */
extern int yylex(void);
extern void yyerror(const char*);

static struct ast* parser_new_resource_node(const char *type, const char *id)
{
	struct ast *node = ast_new(AST_OP_NOOP, id, NULL);
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

static struct branch* branch_new(const char *fact, stringlist *values, unsigned char affirmative)
{
	struct branch *branch;

	branch = malloc(sizeof(struct branch));
	/* FIXME: what if malloc returns NULL? */
	branch->fact = fact;
	branch->values = values;
	branch->affirmative = affirmative;
	branch->then = NULL;
	branch->otherwise = NULL;

	return branch;
}

static void branch_connect(struct branch *branch, struct ast *then, struct ast *otherwise)
{
	if (branch->affirmative) {
		branch->then = then;
		branch->otherwise = otherwise;
	} else {
		branch->then = otherwise;
		branch->otherwise = then;
	}
}

static struct ast* branch_expand_nodes(struct branch *branch)
{
	struct ast *top = NULL, *current = NULL;
	unsigned int i;

	for (i = 0; i < branch->values->num; i++) {
		current = ast_new(AST_OP_IF_EQUAL, branch->fact, branch->values->strings[i]);
		ast_add_child(current, branch->then);
		if (!top) {
			ast_add_child(current, branch->otherwise);
		} else {
			ast_add_child(current, top);
		}
		top = current;
	}
	return top;
}

static struct map* map_new(const char *fact, const char *attr, stringlist *fact_values, stringlist *attr_values, const char *default_value)
{
	struct map *map;

	map = malloc(sizeof(struct map));
	/* FIXME: what if malloc returns NULL? */
	map->fact = fact;
	map->attribute = attr;
	map->fact_values = fact_values;
	map->attr_values = attr_values;
	map->default_value = default_value;

	return map;
}

static struct ast* map_expand_nodes(struct map *map)
{
	struct ast *top = NULL, *current = NULL;
	unsigned int i;

	/* FIXME: what if fact_values->num != attr_values->num */
	for (i = 0; i < map->fact_values->num; i++) {
		current = ast_new(AST_OP_IF_EQUAL, map->fact, map->fact_values->strings[i]);
		ast_add_child(current, ast_new(AST_OP_SET_ATTRIBUTE, map->attribute, map->attr_values->strings[i]));
		if (!top) {
			if (map->default_value) {
				ast_add_child(current, ast_new(AST_OP_SET_ATTRIBUTE, map->attribute, map->default_value));
			} else {
				ast_add_child(current, ast_new(AST_OP_NOOP, NULL, NULL));
			}
		} else {
			ast_add_child(current, top);
		}
		top = current;
	}

	return top;
}

struct ast *spec_result = NULL;

%}

%%

policies:
	{ spec_result = ast_new(AST_OP_PROG, NULL, NULL); }
	| policies policy
	{ ast_add_child(spec_result, $2); }
	;

policy: T_KEYWORD_POLICY T_QSTRING '{' blocks '}'
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

value: T_QSTRING
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

mapped_value: T_QSTRING ':' value
	    { $$[0] = $1;
	      $$[1] = $3; }
	    ;

mapped_value_default:
		    { $$ = NULL }
		    | T_KEYWORD_ELSE ':' value
		    { $$ = $3; }
		    ;

