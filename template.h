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

/** @file template.h */

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
	/** The operation represented by this node */
	enum tnode_type type;

	/** Operation specific data. */
	char *d1;
	/** Operation specific data. */
	char *d2;

	/** Number of child nodes underneath this one. */
	unsigned int size;
	/** Array of child nodes underneath this one. */
	struct tnode **nodes;
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

/**
  Allocate and initialize a new (empty) template.

  @note The pointer returned by this function must be passed
        to template_free in order to properly release all allocated
        memory held by the template.

  @returns a pointer to a heap-allocated template, or NULL on failure.
 */
struct template* template_new(void);

/**
  Parse a template definition.

  @note The pointer returned by this function must be passed
        to template_free in order to properly release all allocated
        memory held by the template.

  @param  path    Path to the template file to parse.
  @param  facts   Hash of facts to (eventually) evaluate the template against.

  @returns a pointer to a heap-allicated template representing \a path,
           or NULL on failure.
 */
struct template* template_create(const char *path, struct hash *facts);

/**
  Free a template.

  @note The template::facts member is not freed by this call.

  @param  t   Template to free.
 */
void template_free(struct template *t);

/**
  Define (or re-define) a local template variable.

  @param  t      Template to define a variable in.
  @param  name   Name of the variable.
  @param  value  Value of the variable.  This must be a string, and
                 will be duplicated via xstrdup.

  @returns 0 on success, non-zero on failure.
 */
int template_add_var(struct template *t, const char *name, const char *value);

/**
  De-reference a variable / fact reference.

  This function first checks the local variables defined by
  template_add_var.  If no value is found in that hash, it
  then checks the associated facts list.

  @param  t      Template to find the variable definition in.
  @param  name   Name of the variable to dereference.

  @returns the string value of the variable or fact if a defined
           value is found; NULL otherwise.
 */
void* template_deref_var(struct template *t, const char *name);

/**
  Render a template

  Renders the template by dereferencing all variable and fact
  values, evaluating conditionals and other template language
  constructs, to produce the template "value".

  @param  t   Template to render

  @returns a character string containing the template "value",
           or NULL on failure.
 */
char* template_render(struct template *t);

/**
  Create a new template node.

  This routine is used internally by the template parser.
  It allocates and initializes a struct tnode according to
  the passed in data.

  @param  t     Template to create the node in
  @param  type  Type of node to create
  @param  d1    Value of the first operation-specific data member
  @param  d2    Value of the second operation-specific data member

  @note for reasons of optimization, the d1 and d2 pointers
        are "taken over" by this function.  Do not free() them
        after a call to template_new_tnode; you risk a double-free.

  @returns a heap-allocated tnode structure, which also exists in
           the template::nodes member.  NULL is returned on failure
           (in which case template::nodes is untouched).
 */
struct tnode* template_new_tnode(struct template *t, enum tnode_type type, char *d1, char *d2);

/**
  Add a template node as a child of another

  @param  parent    Parent of \a child.
  @param  child     Node to add as a child of \a parent.

  @returns 0 on success, non-zero on failure.
 */
int tnode_add(struct tnode *parent, struct tnode *child);

#endif /* _TEMPLATE_H */
