#ifndef _POLICY_H
#define _POLICY_H

#include "list.h"
#include "hash.h"
#include "resource.h"
#include "res_file.h"
#include "res_group.h"
#include "res_user.h"

/** @file policy.h
 */

/** Abstract Syntax Tree node types */
enum oper {
	NOOP = 0,
	PROG,
	IF,
	INCLUDE,
	RESOURCE,
	ATTR
};

/**
  Abstract Syntax Tree Node

  This structure defines a single node, root or otherwise, in an abstract
  syntax tree.  These are used to represent the operations necessary to
  build a manifest of policy and host definitions, and serve as an integral
  step between the policy specification parser and a policy structure.
 */
struct stree {
	/** The operation represented by this node */
	enum oper op;

	/** Operation specific data. */
	char *data1;
	/** Operation specific data. */
	char *data2;

	/** Number of child nodes underneath this one. */
	unsigned int size;
	/** Array of child nodes underneath this one. */
	struct stree **nodes;
};

/**
  Manifest of all known Host and Policy Definitions

  This structure pulls together the abstract syntax trees for all host
  and policy definitions, and organizes them in such a way that they
  can be easily located.
 */
struct manifest {
	/** Policy definitions (stree nodes), hashed by policy name */
	struct hash *policies;

	/** Host definitions (stree nodes), hashed by FQDN */
	struct hash *hosts;

	/** Collapsed list of all nodes in both the manifest::policies
	    and manifest::hosts members, to avoid loops while freeing
	    memory used by the manifest structure. */
	struct stree **nodes;

	/** Number of nodes in the manifest::nodes member. */
	size_t nodes_len;

	/** Pointer to the root abstract syntax tree node representing
	    the entire set of host and policy definitions parsed. */
	struct stree *root;
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
	/** User-assigned name of policy */
	char *name;

	/** List of file resources */
	struct list res_files;

	/** List of group resources */
	struct list res_groups;

	/** List of user resources */
	struct list res_users;

	/* Searchable hash table, keyed "TYPE:pkey" */
	struct hash *resources;
};

struct manifest* manifest_new(void);
void manifest_free(struct manifest *m);
struct stree* manifest_new_stree(struct manifest *m, enum oper op, const char *data1, const char *data2);

int stree_add(struct stree *parent, struct stree *child);
int stree_compare(const struct stree *a, const struct stree *b);

struct hash* fact_read(FILE *io, struct hash *facts);
int fact_parse(const char *line, char **k, char **v);

struct policy* policy_generate(struct stree *root, struct hash *facts);

struct policy* policy_new(const char *name);
void policy_free(struct policy *pol);
void policy_free_all(struct policy *pol);

int policy_add_file_resource(struct policy *pol, struct res_file *rf);
int policy_add_group_resource(struct policy *pol, struct res_group *rg);
int policy_add_user_resource(struct policy *pol, struct res_user *ru);

char* policy_pack(const struct policy *pol);
struct policy* policy_unpack(const char *packed);

#endif /* _POLICY_H */
