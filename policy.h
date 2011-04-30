#ifndef _POLICY_H
#define _POLICY_H

#include "list.h"
#include "hash.h"
#include "resources.h"

/** @file policy.h
 */

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

	/** List of all resources */
	struct list resources;

	/** List of all dependencies */
	struct list dependencies;

	/** Searchable hash table, keyed "TYPE:pkey" */
	struct hash *index;
};

/**
  Iterate over a policy's resources

  @param r    Resource (pointer) to use as an iterator
  @param pol  Policy
 */
#define for_each_resource(r,pol) for_each_node((r),&((pol)->resources), l)

/**
  Iterate (safely) over a policy's resources

  This macro works just like for_each_resource, except that the
  resource \a r can be removed safely from the list without causing
  strange behavior.

  @param r    Resource (pointer) to use as an iterator
  @param t    Resource (pointer) for temporary storage
  @param pol  Policy
 */
#define for_each_resource_safe(r,t,pol) for_each_node_safe((r),(t),&((pol)->resources), l)

/**
  Iterate over a policy's dependencies

  @param d    Dependency (pointer) to use as an iterator
  @param pol  Policy
 */
#define for_each_dependency(d,pol) for_each_node((d),&((pol)->dependencies), l)

/**
  Iterate (safely) over a policy's dependencies

  This macro works just like for_each_dependency, except that the
  dependency \a d can be removed safely from the list without causing
  strange behavior.

  @param d    Dependency (pointer) to use as an iterator
  @param t    Dependency (pointer) for temporary storage
  @param pol  Policy
 */
#define for_each_dependency_safe(d,t,pol) for_each_node_safe((d),(t),&((pol)->dependencies), l)

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

/**
  De-allocate a policy object, and all of the resource
  objects it has references to.

  @param  pol    Policy object to de-allocate.
 */
void policy_free_all(struct policy *pol);

/**
  Add a new Resource to a policy.

  @param  pol    Policy to add the resource to.
  @param  res    Resource to add.

  @returns 0 on success, non-zero on failure.
 */
int policy_add_resource(struct policy *pol, struct resource *res);

/**
  Find a Resource based on its attributes.

  This function is used to find resources defined inside of a policy
  based on a single attribute filter.  For example, to find the res_user
  resource of the user account 'bob':

  @verbatim
  struct resource *bob;
  bob = policy_find_resource(pol, RES_USER, "username", "bob");
  @endverbatim

  Resource normalization, which often involves creating implicit
  dependencies on other resources, makes heavy use of this function.

  @note If more than one resource would match a given attribute
        filter, only the first is returned.

  @param  pol    Policy to search
  @param  type   Type of resource (like RES_SERVICE) to look for
  @param  attr   Name of the Resource Attribute to filter on
  @param  value  Value to look for in \a attr

  @returns a pointer to the first matching resource if found, or
           NULL if no match was found.
 */
struct resource* policy_find_resource(struct policy *pol, enum restype type, const char *attr, const char *value);

/**
  Add a Resource Dependency to a Policy

  @param  pol  Policy to add dependency \a dep to
  @param  dep  Resource Dependency to add

  @returns 0 on success, non-zero on failure.
 */
int policy_add_dependency(struct policy *pol, struct dependency *dep);

/**
  Notify all dependent resources of a change.

  All dependencies is evaluated in turn to determine if
  \a cause if the "b" resource (remembering that "a depends on b").
  For each matching dependency, resource_notify is called on the
  dependent resource, so that it knows it may still need to do something
  even if it is compliant.

  The simplest example is a service (i.e. OpenLDAP) and its configuration
  file.  If the config file changes, the service needs to be reloaded,
  even if it is already running.  In this example, \a cause would be
  the configuration file.

  @param  pol    Policy
  @param  cause  The resource that has changed

  @returns 0 on success or non-zero on failure.  Not finding a matching
           dependency is not classified as a failure.
 */
int policy_notify(const struct policy *pol, const struct resource *cause);

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
