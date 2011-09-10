/*
  Copyright 2011 James Hunt <james@jameshunt.us>

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

#ifndef _TEMPLATE_H
#define _TEMPLATE_H

#include "clockwork.h"
#include "hash.h"

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
	/** Pointer to the root template node representing
	    the complete template definition as parsed. */
	struct tnode *root;

	/** Hash containing the variables defined by this template. */
	struct hash  *vars;

	/** Hash containing the facts (external variables)
	    to interpolate this template against. */
	struct hash  *facts; /* pointer to host facts; unmanaged */

	/** Collapsed list of all template nodes, for memory management. */
	struct tnode **nodes;

	/** Number of nodes in the manifest::nodes member. */
	size_t nodes_len;
};

struct template* template_new(void);
struct template* template_create(const char *path, struct hash *facts);
void template_free(struct template *t);
int template_add_var(struct template *t, const char *name, const char *value);
void* template_deref_var(struct template *t, const char *name);
char* template_render(struct template *t);
struct tnode* template_new_tnode(struct template *t, enum tnode_type type, char *d1, char *d2);
int tnode_add(struct tnode *parent, struct tnode *child);

#endif /* _TEMPLATE_H */
