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

#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "clockwork.h"

/** Template Syntax Tree node types */
enum tnode_type {
	TNODE_NOOP = 0,
	TNODE_ECHO,
	TNODE_VALUE,
	TNODE_REF,
	TNODE_IF_EQ,
	TNODE_IF_NE,
	TNODE_ASSIGN
};

/**
  Template Syntax Tree Node

  This structure defines a single node, root or otherwise, in an
  abstract syntax tree that represents a file template.  When a
  template is interpolated, its syntax tree is walked and each
  operation is evaluated against the variable context for
  interpolation.
 */
struct tnode {
	enum tnode_type type;  /* node type */

	char *d1;              /* operation-specific data */
	char *d2;              /* operation-specific data */

	unsigned int size;     /* number of child nodes */
	struct tnode **nodes;  /* child nodes */
};

/**
  A File Template
 */
struct template {
	struct tnode *root;   /* root of template syntax tree */

	struct hash  *vars;   /* variables defined by the template */
	struct hash  *facts;  /* host facts hash; unmanaged */

	struct tnode **nodes; /* all syntax nodes, for mem. management */
	size_t nodes_len;     /* how many syntax nodes do we have? */
};

struct template* template_new(void);
struct template* template_create(const char *path, struct hash *facts);
void template_free(struct template *t);
int template_add_var(struct template *t, const char *name, const char *value);
void* template_deref_var(struct template *t, const char *name);
char* template_render(struct template *t);
struct tnode* template_new_tnode(struct template *t, enum tnode_type type, char *d1, char *d2);
int tnode_add(struct tnode *parent, struct tnode *child);

#endif
