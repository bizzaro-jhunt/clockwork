#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "ast.h"
#include "fact.h"

struct ast_walker {
	struct policy *policy;
	struct list   *facts;
	unsigned int   context;

	union {
		struct res_user  *user;
		struct res_group *group;
		struct res_file  *file;
	} resource;
};


struct ast* ast_new(unsigned int op, const void *data1, const void *data2)
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

int ast_init(struct ast *ast, unsigned int op, const void *data1, const void *data2)
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

static struct ast* ast_eval_if_equal(struct ast *ast, struct ast_walker *ctx)
{
	assert(ast);
	assert(ctx);

	struct fact *fact;
	const char *value = NULL;

	if (ctx->facts) {
		for_each_node(fact, ctx->facts, facts) {
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

static struct ast* ast_eval_if_not_equal(struct ast *ast,struct ast_walker *ctx)
{
	assert(ast);
	assert(ctx);

	struct ast *tmp;

	tmp = ast->nodes[0];
	ast->nodes[0] = ast->nodes[1];
	ast->nodes[1] = tmp;
	ast->op = AST_OP_IF_EQUAL;

	return ast_eval_if_equal(ast, ctx);
}

static void ast_eval_set_attribute(struct ast *node, struct ast_walker *ctx)
{
	assert(node);
	assert(ctx);

	switch (ctx->context) {
	case AST_OP_DEFINE_RES_USER:
		if (strcmp(node->data1, "uid") == 0) {
			res_user_set_uid(ctx->resource.user, strtoll(node->data2, NULL, 10));
		} else if (strcmp(node->data1, "gid") == 0) {
			res_user_set_gid(ctx->resource.user, strtoll(node->data2, NULL, 10));
		} else if (strcmp(node->data1, "home") == 0) {
			res_user_set_dir(ctx->resource.user, node->data2);
		}
		break;

	case AST_OP_DEFINE_RES_GROUP:
		if (strcmp(node->data1, "name") == 0) {
			res_group_set_name(ctx->resource.group, node->data2);
		} else if (strcmp(node->data1, "gid") == 0) {
			res_group_set_gid(ctx->resource.group, strtoll(node->data2, NULL, 10));
		}
		break;

	case AST_OP_DEFINE_RES_FILE:
		if (strcmp(node->data1, "owner") == 0) {
			res_file_set_uid(ctx->resource.file, 0); /* FIXME: hard-coded UID */
		} else if (strcmp(node->data1, "group") == 0) {
			res_file_set_gid(ctx->resource.file, 0); /* FIXME: hard-coded GID */
		} else if (strcmp(node->data1, "lpath") == 0) {
			res_file_set_path(ctx->resource.file, node->data2);
		} else if (strcmp(node->data1, "mode") == 0) {
			res_file_set_mode(ctx->resource.file, strtoll(node->data2, NULL, 0));
		} else if (strcmp(node->data1, "source") == 0) {
			res_file_set_source(ctx->resource.file, node->data2);
		}
		break;
	}
}

static int ast_evaluate_subtree(struct ast *node, struct ast_walker *ctx)
{
	assert(node);
	assert(ctx);

	unsigned int i;

again:
	switch (node->op) {
	case AST_OP_IF_EQUAL:
		node = ast_eval_if_equal(node, ctx);
		goto again;

	case AST_OP_IF_NOT_EQUAL:
		node = ast_eval_if_not_equal(node, ctx);
		goto again;

	case AST_OP_DEFINE_RES_USER:
		ctx->context = AST_OP_DEFINE_RES_USER;
		ctx->resource.user = res_user_new();
		res_user_set_name(ctx->resource.user, node->data1);
		list_add_tail(&(ctx->resource.user->res), &(ctx->policy->res_users));
		break;

	case AST_OP_DEFINE_RES_GROUP:
		ctx->context = AST_OP_DEFINE_RES_GROUP;
		ctx->resource.group = res_group_new();
		res_group_set_name(ctx->resource.group, node->data1);
		list_add_tail(&(ctx->resource.group->res), &(ctx->policy->res_groups));
		break;
	
	case AST_OP_DEFINE_RES_FILE:
		ctx->context = AST_OP_DEFINE_RES_FILE;
		ctx->resource.file = res_file_new();
		res_file_set_path(ctx->resource.file, node->data1);
		list_add_tail(&(ctx->resource.file->res), &(ctx->policy->res_files));
		break;

	case AST_OP_SET_ATTRIBUTE:
		ast_eval_set_attribute(node, ctx);
		break;

	case AST_OP_PROG:
		break;

	case AST_OP_NOOP:
		break;
	}

	for (i = 0; i < node->size; i++) {
		if (ast_evaluate_subtree(node->nodes[i], ctx) != 0) {
			return -1;
		}
	}

	return 0;
}

struct policy *ast_evaluate(struct ast *ast, struct list *facts)
{
	assert(ast);

	struct ast_walker ctx;
	unsigned int i;

	ctx.facts = facts;
	ctx.policy = policy_new(ast->data1, policy_latest_version());
	ctx.context = AST_OP_DEFINE_POLICY;

	/* FIXME: check result of policy_new */

	for (i = 0; i < ast->size; i++) {
		ast_evaluate_subtree(ast->nodes[i], &ctx);
	}

	return ctx.policy;
}
