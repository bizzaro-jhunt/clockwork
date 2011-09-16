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

#ifndef POLICY_H
#define POLICY_H

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
	POLICY
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
	struct hash *policies;  /* policy defs, hashed by name */
	struct hash *hosts;     /* host defs, hashed by FQDN */

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
	char *name;               /* policy name */

	struct list resources;    /* resources defined fror policy */
	struct list dependencies; /* resource dependencies (implicit and explicit) */

	struct hash *index;       /* resources, keyed by "TYPE:pkey" */
};

/* Iterate over a policy's resources */
#define for_each_resource(r,pol) for_each_node((r),&((pol)->resources), l)

/* Iterate (safely) over a policy's resources */
#define for_each_resource_safe(r,t,pol) for_each_node_safe((r),(t),&((pol)->resources), l)

/* Iterate over a policy's dependencies */
#define for_each_dependency(d,pol) for_each_node((d),&((pol)->dependencies), l)

/* Iterate (safely) over a policy's dependencies */
#define for_each_dependency_safe(d,t,pol) for_each_node_safe((d),(t),&((pol)->dependencies), l)

struct manifest* manifest_new(void);
void manifest_free(struct manifest *m);

struct stree* manifest_new_stree(struct manifest *m, enum oper op, char *data1, char *data2);
int stree_add(struct stree *parent, struct stree *child);
int stree_compare(const struct stree *a, const struct stree *b);

struct hash* fact_read(FILE *io, struct hash *facts);
int fact_write(FILE *io, struct hash *facts);
int fact_parse(const char *line, struct hash *hash);

struct policy* policy_generate(struct stree *root, struct hash *facts);
struct policy* policy_new(const char *name);
void policy_free(struct policy *pol);
void policy_free_all(struct policy *pol);
int policy_add_resource(struct policy *pol, struct resource *res);
struct resource* policy_find_resource(struct policy *pol, enum restype type, const char *attr, const char *value);
int policy_add_dependency(struct policy *pol, struct dependency *dep);
int policy_notify(const struct policy *pol, const struct resource *cause);

char* policy_pack(const struct policy *pol);
struct policy* policy_unpack(const char *packed);

#endif
