#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ast.h"
#include "fact.h"

struct ast* ast_new(unsigned int op, void *data1, void *data2)
{
	struct ast *ast = malloc(sizeof(struct ast));
	if (!ast) {
		return NULL;
	}

	if (ast_init(ast, op, data1, data2) != 0) {
		free(ast);
		return NULL;
	}

	return ast;
}

int ast_init(struct ast *ast, unsigned int op, void *data1, void *data2)
{
	assert(ast);

	ast->op = op;
	ast->data1 = data1;
	ast->data2 = data2;

	ast->size = 0;
	ast->nodes = NULL;

	ast->refs = 0;

	return 0;
}

int ast_deinit(struct ast *ast)
{
	if (!ast || ast->refs > 0) {
		ast->refs--;
		return -1;
	}

	ast->data1 = NULL;
	ast->data2 = NULL;

	ast->size = 0;
	free(ast->nodes);

	return 0;
}

void ast_free(struct ast *ast)
{
	if (ast_deinit(ast) != 0) {
		return;
	}

	free(ast);
}

void ast_free_all(struct ast *ast)
{
	if (!ast) {
		return;
	}

	unsigned int i;

	for (i = 0; i < ast->size; i++) {
		ast_free_all(ast->nodes[i]);
	}

	ast_free(ast);
}

int ast_add_child(struct ast *parent, struct ast *child)
{
	assert(parent);

	struct ast **n;

	if (!child) {
		return -1;
	}

	n = realloc(parent->nodes, sizeof(struct ast*) * (parent->size + 1));
	if (!n) {
		return -1;
	}

	parent->nodes = n;
	parent->nodes[parent->size++] = child;

	return 0;
}

int ast_compare(struct ast *a, struct ast *b)
{
	unsigned int i;

	if (!a || !b) {
		return 1;
	}

	if (a->op != b->op
	 || a->size != b->size
	 || (a->data1 && !b->data1) || (!a->data1 && b->data1)
	 || (a->data2 && !b->data2) || (!a->data2 && b->data2)
	 || (a->data1 && strcmp(a->data1, b->data1) != 0)
	 || (a->data2 && strcmp(a->data2, b->data2) != 0)) {
		return 1;
	}

	for (i = 0; i < a->size; i++) {
		if (ast_compare(a->nodes[i], b->nodes[i]) != 0) {
			return 1;
		}
	}

	return 0;
}

static struct ast* ast_eval(struct ast *ast, struct list *facts);
static struct ast* ast_eval_if_equal(struct ast *ast, struct list *facts);
static struct ast* ast_eval_if_not_equal(struct ast *ast, struct list *facts);

static struct ast* ast_eval_if_equal(struct ast *ast, struct list *facts)
{
	assert(ast);

	struct fact *fact;
	const char *value = NULL;

	if (facts) {
		for_each_node(fact, facts, facts) {
			if (strcmp(fact->name, ast->data1) == 0) {
				value = fact->value;
				break;
			}
		}
	}

	if (value && strcmp(value, ast->data2) == 0) {
		return ast->nodes[0];
	} else {
		return ast->nodes[1];
	}
}

static struct ast* ast_eval_if_not_equal(struct ast *ast, struct list *facts)
{
	assert(ast);

	struct ast *tmp;

	tmp = ast->nodes[0];
	ast->nodes[0] = ast->nodes[1];
	ast->nodes[1] = tmp;
	ast->op = AST_OP_IF_EQUAL;

	return ast_eval(ast, facts);
}

static struct ast* ast_eval(struct ast *ast, struct list *facts)
{
	assert(ast);

again:
	switch (ast->op) {
	case AST_OP_IF_EQUAL:
		ast = ast_eval_if_equal(ast, facts);
		goto again;

	case AST_OP_IF_NOT_EQUAL:
		ast = ast_eval_if_not_equal(ast, facts);
		goto again;
	}

	return ast;
}

struct policy* ast_define_policy(struct ast *ast, struct list *facts)
{
	assert(ast);

	struct policy *policy;
	struct ast *next;
	unsigned int i;
	struct res_user *ru;
	struct res_group *rg;
	struct res_file *rf;

	if (ast->op != AST_OP_DEFINE_POLICY) {
		return NULL;
	}

	policy = policy_new(ast->data1, 0);

	for (i = 0; i < ast->size; i++) {
		next = ast_eval(ast->nodes[i], facts);
		switch (next->op) {
		case AST_OP_NOOP:
			break;
		case AST_OP_DEFINE_RESOURCE:
			if (strcmp(next->data1, "res_user") == 0) {
				ru = ast_define_res_user(next, facts);
				if (ru) {
					list_add_tail(&ru->res, &policy->res_users);
				}
			} else if (strcmp(next->data1, "res_group") == 0) {
				rg = ast_define_res_group(next, facts);
				if (rg) {
					list_add_tail(&rg->res, &policy->res_groups);
				}
			} else if (strcmp(next->data1, "res_file") == 0) {
				rf = ast_define_res_file(next, facts);
				if (rf) {
					list_add_tail(&rf->res, &policy->res_files);
				}
			} else {
				fprintf(stderr, "SEMANTIC ERROR: Unknown resource type '%s'\n", (const char*)next->data1);
			}
		}
	}

	return policy;
}

struct res_user* ast_define_res_user(struct ast *ast, struct list *facts)
{
	assert(ast);

	struct res_user *ru;
	struct ast *next;
	unsigned int i;

	if (ast->op != AST_OP_DEFINE_RESOURCE
	 && strcmp(ast->data1, "res_user") != 0) {
		return NULL;
	}

	ru = res_user_new();

	for (i = 0; i < ast->size; i++) {
		next = ast_eval(ast->nodes[i], facts);
		switch (next->op) {
		case AST_OP_NOOP: break;
		case AST_OP_SET_ATTRIBUTE:
			if (strcmp(next->data1, "uid") == 0) {
				res_user_set_uid(ru, strtoll(next->data2, NULL, 10));
			} else if (strcmp(next->data1, "gid") == 0) {
				res_user_set_gid(ru, strtoll(next->data2, NULL, 10));
			} else if (strcmp(next->data1, "home") == 0) {
				res_user_set_dir(ru, next->data2);
			}
			break;
		default:
			fprintf(stderr, "ast_define_res_user: SEMANTIC ERROR: unexpected op(%u)\n", next->op);
			res_user_free(ru);
			return NULL;
		}
	}

	return ru;
}

struct res_group* ast_define_res_group(struct ast *ast, struct list *facts)
{
	assert(ast);

	struct res_group *rg;
	struct ast *next;
	unsigned int i;

	if (ast->op != AST_OP_DEFINE_RESOURCE
	 && strcmp(ast->data1, "res_group") != 0) {
		return NULL;
	}

	rg = res_group_new();
	for (i = 0; i < ast->size; i++) {
		next = ast_eval(ast->nodes[i], facts);
		switch (next->op) {
		case AST_OP_NOOP: break;
		case AST_OP_SET_ATTRIBUTE:
			if (strcmp(next->data1, "name") == 0) {
				res_group_set_name(rg, next->data2);
			} else if (strcmp(next->data1, "gid") == 0) {
				res_group_set_gid(rg, strtoll(next->data2, NULL, 10));
			}
			break;
		default:
			fprintf(stderr, "ast_define_res_group: SEMANTIC ERROR: unexpected op(%u)\n", next->op);
			res_group_free(rg);
			return NULL;
		}
	}

	return rg;
}

struct res_file* ast_define_res_file(struct ast *ast, struct list *facts)
{
	assert(ast);

	struct res_file *rf;
	struct ast *next;
	unsigned int i;

	if (ast->op != AST_OP_DEFINE_RESOURCE
	 && strcmp(ast->data1, "res_file") != 0) {
		return NULL;
	}

	rf = res_file_new();

	for (i = 0; i < ast->size; i++) {
		next = ast_eval(ast->nodes[i], facts);
		switch (next->op) {
		case AST_OP_NOOP: break;
		case AST_OP_SET_ATTRIBUTE:
			if (strcmp(next->data1, "owner") == 0) {
				res_file_set_uid(rf, 0);
			} else if (strcmp(next->data1, "group") == 0) {
				res_file_set_gid(rf, 0);
			} else if (strcmp(next->data1, "lpath") == 0) {
				rf->rf_lpath = strdup(next->data2);
			} else if (strcmp(next->data1, "mode") == 0) {
				res_file_set_mode(rf, strtoll(next->data2, NULL, 0));
			} else if (strcmp(next->data1, "source") == 0) {
				rf->rf_rpath = strdup(next->data2); /* FIXME: not the way to do this... */
			}
			break;
		default:
			fprintf(stderr, "ast_define_res_file: SEMANTIC ERROR: unexpected op(%u)\n", next->op);
			res_file_free(rf);
			return NULL;

		}
	}

	return rf;
}

