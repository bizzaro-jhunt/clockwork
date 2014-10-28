/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

#ifndef POLICY_H
#define POLICY_H

#include "mesh.h"
#include "resources.h"

/** Abstract Syntax Tree node types */
enum oper {
	NOOP = 0,
	PROG,
	IF,
	INCLUDE,
	RESOURCE,
	ATTR,
	RESOURCE_ID,
	DEPENDENCY,
	HOST,
	POLICY,

	EXPR_NOOP,
	EXPR_VAL,
	EXPR_FACT,
	EXPR_REGEX,

	EXPR_AND,
	EXPR_OR,
	EXPR_NOT,
	EXPR_EQ,
	EXPR_MATCH,

	ACL,
	ACL_SUBJECT,
	ACL_COMMAND,
};

/**
  Abstract Syntax Tree Node

  This structure defines a single node, root or otherwise, in an abstract
  syntax tree.  These are used to represent the operations necessary to
  build a manifest of policy and host definitions, and serve as an integral
  step between the policy specification parser and a policy structure.
 */
struct stree {
	enum oper op;    /* type of operation */

	char *data1;     /* operation-specific data */
	char *data2;     /* operation-specific data */

	unsigned int   size;  /* how many child nodes? */
	struct stree **nodes; /* array of child nodes */
};

/**
  Manifest of all known Host and Policy Definitions

  This structure pulls together the abstract syntax trees for all host
  and policy definitions, and organizes them in such a way that they
  can be easily located.
 */
struct manifest {
	hash_t *policies;       /* policy defs, hashed by name */
	hash_t *hosts;          /* host defs, hashed by FQDN */

	struct stree *fallback; /* host def for implicit hosts */

	struct stree **nodes;   /* all nodes, for memory management */
	size_t nodes_len;       /* number of nodes */

	struct stree *root;     /* root node of the whole syntax tree */
};

/**
  Policy Definition

  A policy consists of a set of resources, and the attributes that
  must be enforced for each.

  Resources are stored in two ways: a sequential list (by type) that
  reflects the order in which the resources were defined, and a hash
  table containing all resources (keyed by "TYPE:pkey").

  The lists are there so that each resource type can be walked, and
  the hash exists to ease searching.
 */
struct policy {
	char *name;          /* policy name */

	list_t resources;    /* resources defined for policy */
	list_t dependencies; /* resource dependencies (implicit and explicit) */
	list_t acl;          /* remote ACLs defined via policy */

	hash_t *index;       /* resources, keyed by "TYPE:pkey" */
	hash_t *cache;       /* search cache, keyed by "TYPE:attr=val" */
};

/* Iterate over a policy's resources */
#define for_each_resource(r,pol) for_each_object((r),&((pol)->resources), l)

/* Iterate (safely) over a policy's resources */
#define for_each_resource_safe(r,t,pol) for_each_object_safe((r),(t),&((pol)->resources), l)

/* Iterate over a policy's dependencies */
#define for_each_dependency(d,pol) for_each_object((d),&((pol)->dependencies), l)

/* Iterate (safely) over a policy's dependencies */
#define for_each_dependency_safe(d,t,pol) for_each_object_safe((d),(t),&((pol)->dependencies), l)

/* Iterate over a policy ACL */
#define for_each_acl(a,pol) for_each_object((a), &((pol)->acl), l)

/* Iterate (safely) over a policy ACL */
#define for_each_acl_safe(a,t,pol) for_each_object_safe((a), (t), &((pol)->acl), l)

struct manifest* manifest_new(void);
void manifest_free(struct manifest *m);
int manifest_validate(struct manifest *m);

struct stree* manifest_new_stree(struct manifest *m, enum oper op, char *data1, char *data2);
struct stree* manifest_new_stree_expr(struct manifest *m, enum oper op, struct stree *a, struct stree *b);
int stree_add(struct stree *parent, struct stree *child);
int stree_compare(const struct stree *a, const struct stree *b);

hash_t* fact_read(FILE *io, hash_t *facts);
hash_t* fact_read_string(const char *s, hash_t *facts);
int fact_write(FILE *io, hash_t *facts);
int fact_parse(const char *line, hash_t *hash);
int fact_exec_read(const char *script, hash_t *facts);
int fact_cat_read(const char *file, hash_t *facts);
int fact_gather(const char *paths, hash_t *facts);

struct policy* policy_generate(struct stree *root, hash_t *facts);
struct policy* policy_new(const char *name);
void policy_free(struct policy *pol);
void policy_free_all(struct policy *pol);
int policy_add_resource(struct policy *pol, struct resource *res);
struct resource* policy_find_resource(struct policy *pol, enum restype type, const char *attr, const char *value);
int policy_add_dependency(struct policy *pol, struct dependency *dep);

int policy_gencode(const struct policy *pol, FILE *io);

#endif
