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

	/** Searchable hash table, keyed "TYPE:pkey" */
	struct hash *resources;
};

/**
  Allocate and initialize a new manifest.

  @note The pointer returned by this function must be passed
        to manifest_free in order to properly release all allocated
        memory held by the manifest.

  @returns a pointer to a heap-allocated manifest structure, or
           NULL on failure.
 */
struct manifest* manifest_new(void);

/**
  Free a manifest.

  @param  m    Manifest to free.
 */
void manifest_free(struct manifest *m);

/**
  Allocate a new abstract syntax tree node, in the context of
  a manifest.

  This function not only allocates and initializes an stree node,
  it also stores a reference to the allocate stree node, so that
  manifest_free can properly de-allocate the node.  For this reason,
  it is not advised to allocate stree nodes for use in a manifest
  through any other mechanism.

  @param  m        Manifest to create a new stree node for.
  @param  op       Type of operation this node represents.
  @param  data1    Operation specific data.
  @param  data2    Operation specific data.

  @returns a heap-allocated stree structure, containing \a op, \a data1
           and \a data2 on success, or NULL on failure.
 */
struct stree* manifest_new_stree(struct manifest *m, enum oper op, const char *data1, const char *data2);

/**
  Add one abstract syntax tree node as a child of another.

  This function only adds references, it does not duplicate \a child
  or \a parent.

  @note Both \a parent and \a child should already be referenced in
        the manifest; they should have been created by a call to
        manifest_new_stree.

  @param  parent    Parent abstract syntax tree node.
  @param  child     Child node, to add to \a parent.

  @returns 0 on success, non-zero on failure.
 */
int stree_add(struct stree *parent, struct stree *child);

/**
  Compare two abstract syntax trees.

  If either \a a or \a b are NULL, this function will always return
  non-zero, to indicate lack of equality.

  If \a a does not contain the same node layout (parentage, number of
  children, etc.) as \a b, then the two trees are considered not equal,
  and this function will return non-zero.

  When comparing trees, the following fields are considered:
  - stree::op - Integer comparison
  - stree::data1 - String comparison (or NULL equality)
  - stree::data2 - String comparison (or NULL equality)
  - stree::nodes - Recursive call to stree_compare.

  @param  a    An stree node to compare.
  @param  b    Another stree node to compare.

  @returns 0 if \a a is equivalent to \a b, or non-zero otherwise.
 */
int stree_compare(const struct stree *a, const struct stree *b);

/**
  Read facts from a FILE IO stream.

  This function reads a set of facts, in key=value\\n form,
  from the given FILE object, storing them in \a facts.  Parsing
  stops when \a io encounters an EOF condition.

  @param  io       FILE object to read from.
  @param  facts    Pointer to a hash to store facts in.

  @returns A pointer to \a facts, with all new facts added, on success,
           or NULL on failure.
 */
struct hash* fact_read(FILE *io, struct hash *facts);

/**
  Parse a single fact, from a key=value\\n line buffer.

  This function parses a single fact, placing the key in \a k, and
  the value in \a v.

  @see fact_read for more useful fact handling.

  @param  line    NULL-terminated line buffer
  @param  k       Value-result argument to store the parsed key in.
  @param  v       Value-result argument to store the parsed value in.

  @returns 0 on success, non-zero on failure.
 */
int fact_parse(const char *line, char **k, char **v);

/**
  Generate a policy, given a set of facts.

  This function is vital to the proper operation of Clockworks
  policyd Policy Master Daemon.  It turns an abstract syntax tree
  pointed to by \a root, and evaluates all conditional and map
  constructs against a set of facts.

  @note The pointer returned by this function must be passed to
        policy_free in order to release memory resources.

  @param  root     Root of te abstract syntax tree representing the policy.
  @param  facts    Hash of facts to use for conditional evaluation.

  @returns a heap-allocated policy object on success, or NULL on failure.
 */
struct policy* policy_generate(struct stree *root, struct hash *facts);

/**
  Allocate and initialize a new policy.

  @param  name    Name for the new policy.

  @returns a heap-allocated policy object on success, or NULL on failure.
 */
struct policy* policy_new(const char *name);

/**
  De-allocate a policy object.

  @param  pol    Policy object to de-allocate.
 */
void policy_free(struct policy *pol);

void policy_free_all(struct policy *pol);

/**
  Add a new File Resource to a policy.

  @param  pol    Policy to add to.
  @param  rf     File resource to add.

  @returns 0 on succes, non-zero on failure.
 */
int policy_add_file_resource(struct policy *pol, struct res_file *rf);

/**
  Add a new Group Resource to a policy.

  @param  pol    Policy to add to.
  @param  rg     Group resource to add.

  @returns 0 on succes, non-zero on failure.
 */
int policy_add_group_resource(struct policy *pol, struct res_group *rg);

/**
  Add a new User Resource to a policy.

  @param  pol    Policy to add to.
  @param  ru     User resource to add.

  @returns 0 on succes, non-zero on failure.
 */
int policy_add_user_resource(struct policy *pol, struct res_user *ru);

/**
  Serialize a policy into a string representation.

  The string returned by this function should be free by the caller,
  using the standard free(3) standard library function.

  The final serialized form can be passed to policy_unpack to get
  the original policy structure back.

  @param  pol    Policy to serialize.

  @returns a heap-allocated string containing the packed representation
           of the given policy structure.
 */
char* policy_pack(const struct policy *pol);

/**
  Unserialize a policy from its string representation.

  @note The pointer returned by this function should be passed to
        policy_free to de-allocate and release its memory resources.

  @param  packed    Packed (serialized) policy representation.

  @returns a heap-allocated policy structure, based on the information
           found in the \a packed string.
 */
struct policy* policy_unpack(const char *packed);

#endif /* _POLICY_H */
