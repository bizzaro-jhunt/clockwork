#include <stdio.h>
#include <string.h>

#include "../ast.h"
#include "../stringlist.h"
#include "parser.h"

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

static parser_branch* branch_new(const char *fact, stringlist *values, unsigned char affirmative)
{
	parser_branch *branch;

	branch = malloc(sizeof(parser_branch));
	/* FIXME: what if malloc returns NULL? */
	branch->fact = fact;
	branch->values = values;
	branch->affirmative = affirmative;
	branch->then = NULL;
	branch->otherwise = NULL;

	return branch;
}

static void branch_connect(parser_branch *branch, struct ast *then, struct ast *otherwise)
{
	if (branch->affirmative) {
		branch->then = then;
		branch->otherwise = otherwise;
	} else {
		branch->then = otherwise;
		branch->otherwise = then;
	}
}

static struct ast* branch_expand_nodes(parser_branch *branch)
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

static parser_map* map_new(const char *fact, const char *attr, stringlist *fact_values, stringlist *attr_values, const char *default_value)
{
	parser_map *map;

	map = malloc(sizeof(parser_map));
	/* FIXME: what if malloc returns NULL? */
	map->fact = fact;
	map->attribute = attr;
	map->fact_values = fact_values;
	map->attr_values = attr_values;
	map->default_value = default_value;

	return map;
}

static struct ast* map_expand_nodes(parser_map *map)
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

