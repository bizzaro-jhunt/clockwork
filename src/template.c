/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "template.h"
#include "tpl/parser.h"

#include <ctype.h>

struct template_context {
	struct string *out;
	int echo;
};

/***********************************************************************/

static void tnode_free(struct tnode *n)
{
	if (n) {
		free(n->d1);
		free(n->d2);
		free(n->nodes);
	}
	free(n);
}

static int _template_render(struct template *t, struct tnode *n, struct template_context *ctx)
{
	unsigned int i;

again:
	switch (n->type) {
	case TNODE_NOOP:
		break;

	case TNODE_ECHO:
		ctx->echo = 1;
		for (i = 0; i < n->size; i++) {
			_template_render(t, n->nodes[i], ctx);
		}
		ctx->echo = 0;
		return 0;

	case TNODE_VALUE:
		if (ctx->echo) { string_append(ctx->out, n->d1); }
		return 0;

	case TNODE_REF:
		if (ctx->echo) { string_append(ctx->out, template_deref_var(t, n->d1)); }
		return 0;

	case TNODE_IF_EQ:
		n = (xstrcmp(n->d2, template_deref_var(t, n->d1)) == 0 ? n->nodes[0] : n->nodes[1]);
		goto again;

	case TNODE_IF_NE:
		n = (xstrcmp(n->d2, template_deref_var(t, n->d1)) != 0 ? n->nodes[0] : n->nodes[1]);
		goto again;

	case TNODE_ASSIGN:
		if (ctx->echo) { string_append(ctx->out, n->d2); }
		template_add_var(t, n->d1, n->d2);
		return 0;

	default:
		ERROR("Bad node type: %i", n->type);
		return -1;
	}

	for (i = 0; i < n->size; i++) {
		_template_render(t, n->nodes[i], ctx);
	}
	return 0;
}

/***********************************************************************/

/**
  Create a new (empty) template.

  The pointer returned by this function must be freed via
  @template_free.

  On success, returns a new template.
  On failure, returns NULL.
 */
struct template* template_new(void)
{
	struct template *t = xmalloc(sizeof(struct template));

	t->vars = hash_new();
	t->nodes = NULL;
	t->nodes_len = 0;

	t->root = NULL;

	return t;
}

/**
  Parse a template from $file.

  The template will be parsed into a `template` structure, and the
  $facts hash will be assigned to the template for variable
  interpolation.

  The pointer returned by this function must be freed via
  @template_free.

  On success, returns a pointer to a new template.
  On failure, returns NULL.
 */
struct template* template_create(const char *path, struct hash *facts)
{
	struct template *t = parse_template(path);
	if (t) {
		t->facts = facts;
	}
	return t;
}

/**
  Free template $t.

  The `facts` hash held by the template is *not* freed.
  Memory management for that component is left to the caller.
 */
void template_free(struct template *t)
{
	size_t i;

	if (t) {
		for (i = 0; i < t->nodes_len; i++) { tnode_free(t->nodes[i]); }
		free(t->nodes);
		/* free variable store; including values */
		hash_free_all(t->vars);
		/* don't free t->facts; it's not ours */
	}
	free(t);
}

/**
  Set variable $name on $t to $value.

  On success, returns 0. On failure, returns non-zero.
 */
int template_add_var(struct template *t, const char *name, const char *value)
{
	hash_set(t->vars, name, xstrdup(value));
	return 0;
}

/**
  Dereference variable $name on $t.

  This function first checks the local variables defined by
  @template_add_var.  If no value is found in that hash, it
  then checks the associated facts list.

  If the variable exists, its value is returned.  If the variable
  does not exist, but the fact does, the fact's value is returned.

  If neither exists, NULL is returned.
 */
void* template_deref_var(struct template *t, const char *name)
{
	void *v;
	v = hash_get(t->vars, name);
	if (!v) { v = hash_get(t->facts, name); }
	return v;
}

/**
  Render template $t.

  Renders the template by dereferencing all variable and fact
  values, evaluating conditionals and other template language
  constructs, to produce the template "value".

  On success, returns a string containing the rendered template.
  On failure, returns NULL.
 */
char* template_render(struct template *t)
{
	char *data;
	struct template_context ctx;
	ctx.echo = 0;
	ctx.out = string_new(NULL, 0);

	_template_render(t, t->root, &ctx);

	data = xstrdup(ctx.out->raw);
	string_free(ctx.out);
	return data;
}

/**
  Create a new template node.

  This routine is used internally by the template parser.
  It allocates and initializes a struct tnode according to
  the passed in data.

  **Note:** for reasons of optimization, the d1 and d2 pointers
  are "taken over" by this function.  Do not free() them
  after a call to template_new_tnode; you risk a double-free.

  On success, returns a new `tnode`.
  On failure, returns NULL.
 */
struct tnode* template_new_tnode(struct template *t, enum tnode_type type, char *d1, char *d2)
{
	struct tnode *node;
	struct tnode **nodes;

	node = xmalloc(sizeof(struct tnode));
	nodes = realloc(t->nodes, sizeof(struct node*) * (t->nodes_len + 1));

	if (!node || !nodes) {
		free(node);
		free(nodes);
		return NULL;
	}

	node->type = type;
	node->d1 = d1;
	node->d2 = d2;

	nodes[t->nodes_len++] = node;
	t->nodes = nodes;

	return node;
}

/**
  Add $child to $parent.

  On success, returns 0.  On failure, returns non-zero.
 */
int tnode_add(struct tnode *parent, struct tnode *child)
{
	struct tnode **list;

	if (!parent || !child) { return -1; }

	list = realloc(parent->nodes, sizeof(struct tnode*) * (parent->size + 1));
	if (!list) { return -1; }

	list[parent->size++] = child;
	parent->nodes = list;

	return 0;
}

