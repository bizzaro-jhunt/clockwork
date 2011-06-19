#ifndef RESOURCE_H
#define RESOURCE_H

#include "clockwork.h"

#include <augeas.h>

#include "list.h"
#include "userdb.h"
#include "hash.h"

#include "managers/package.h"
#include "managers/service.h"

/** @file resource.h
 */

/** Resource Type Identifiers */
enum restype {
	RES_USER,
	RES_GROUP,
	RES_FILE,
	RES_PACKAGE,
	RES_SERVICE,
	RES_HOST,
	RES_UNKNOWN /* must be the LAST enumerated value */
};

/**
  Represents the full environment needed by all resources for fixups.

  For example, when fixing up user resources, res_user needs access to
  the password and shadow databases to create, update and remove accounts.
  These databases (pwdb and spdb structures) are stored in the resource
  environment and shared with all resources.

  As new resources are implemented, the resource_env structure will
  have to be updated with new members.
 */
struct resource_env {
	/** Package Manager used by res_package */
	const struct package_manager *package_manager;
	/** Service Manager used by res_service */
	const struct service_manager *service_manager;

	/** Parsed /etc/passwd database, for res_user */
	struct pwdb *user_pwdb;
	/** Parsed /etc/shadow database, for res_user */
	struct spdb *user_spdb;

	/** Parsed /etc/group database, for res_group */
	struct grdb *group_grdb;
	/** Parsed /etc/gshadow database, for res_group */
	struct sgdb *group_sgdb;

	/** Augeas context for sub-file configuration edit support */
	augeas *aug_context;

	/** File descriptor used by res_file to refresh local files
	    from remote copies */
	int file_fd;
	/** Number of bytes to read from file_fd before EOF is reached */
	ssize_t file_len;
};

struct policy;

/**
  A Resource

  Resources are abstrac representation of specific "things" that
  Clockwork manages, like users, group and files.  This structure
  exists to provide a single unified view of all resources as
  a single data type.
 */
struct resource {
	/** Type of resource represented. */
	enum restype type;

	/** Opaque pointer to the res_* structure that backs
	    this resource.  Resource callback functions are
	    respsonsible for casting this pointer to the
	    appropriate type. */
	void *resource;

	/** Fully-qualified Resource Identifier */
	char *key;

	/** Other resources that this resource depends on */
	struct resource **deps;
	/** Number of dependencies */
	int ndeps;

	/** Node for creating lists of resources (primarily used
	    by the policy structure) */
	struct list l;
};

/**
  A Resource Dependency

  Dependencies (either explicit or implicit) chart relationships
  between two resources, and enable the transformation of
  declarative "what should be" descriptions of system state into
  imperative "what to do" plans.

  For example, the Apache Web Server depends on its configuration
  files.  If those files are updated, the service needs to be
  reloaded.  This is an explicit dependency that service("apache")
  has on file("/etc/httpd/httpd.conf").

  Implicit dependencies are used internally to ensure that actions
  are carried out in the proper sequence.  For example, in order to
  create a new file belonging to the user 'bob', Clockwork must
  ensure that the user 'bob' exists before attempting to chown the
  file.  Implicit dependencies are created internally during the
  resource normalization process.

  Member names are based on a simple rule: a depends on b.
 */
struct dependency {
	/** Resouce Identifier of the dependent resource */
	char *a;
	/** Resource Identifier of the resource that \a depends on */
	char *b;

	/** Resource structure of the dependent resource */
	struct resource *resource_a;

	/** Resource structure of the resource that \a depends on */
	struct resource *resource_b;

	/** Node for creating lists of dependencies (primarily used
	    by the policy structure) */
	struct list l;
};

/**
  Check if an attribute is enforced on \a r.

  @param  r  Resource to check
  @param  a  Attribute (RES_*_* contstant)
  @returns non-zero if \a a is enforced; zero if not.
 */
#define ENFORCED(r,a)  (((r)->enforced  & a) == a)

/**
  Check if attribute \a a is out of compliance.

  @param  r  Resource to check
  @param  a  Attribute (RES_*_* contstant)
  @returns non-zero if \a a is out of compliance; zero if not.
 */
#define DIFFERENT(r,a) (((r)->different & a) == a)

/**
  Allocate and initialize a new resource.

  Since this function is primarily used by the spec parser, the
  type parameter is given as a resource type name string, like
  "user" or "service".

  @note This function calls the res_*_new resource callback
        appropriate to the type of resource to be created.

  The pointer returned must be freed by a call to resource_free.

  @param  type   Type of resource to create
  @param  key    Unqualified identifier for new resource

  @returns a pointer to a heap-allocated resource structure, with
           all of its members properly initialized to sane default
           values, or NULL on failure.
 */
struct resource* resource_new(const char *type, const char *key);

/**
  Free a dynamically allocated resource structure (\a r)

  @note This function calls the res_*_free resource callback
        appropriate to the type of resource \a r.

  @param  r   Resource to free
 */
void resource_free(struct resource *r);

/**
  Fully-Qualified Resource Identifier

  This function generates a fully-qualified resource identifier
  for the given resource \a r, based on its type and unqualified
  identifier.

  @note The string pointer returned must be freed by the caller.

  @param  r  Resource to generate a RID for

  @return a fully-qualified resource identifier, or NULL on failure.
 */
char *resource_key(const struct resource *r);

/**
  Normalize a Resource

  Some resources need additional processing beyond setting of
  attribute values.  This function provides implementors a
  hook into the policy parsing / reconstitution process so that
  these normalizations can be performed before the resource
  goes into "fix up" mode.

  @note This function calls the res_*_norm resource callback
        appropriate to the type of resource \a r.

  The \a pol parameter allows the resource implementation to
  locate other resources for implicit dependency injection.
  It is assumed that by the time resource_norm is called, all
  resource definitions have been added to the policy.

  @param  r    Resource to normalize
  @param  pol  Policy to normalize against

  @returns 0 on succes, non-zero on failure.
 */
int resource_norm(struct resource *r, struct policy *pol, struct hash *facts);

/**
  Set a Resource Attribute

  @note This function calls the res_*_set resource callback
        appropriate to the type of resource \a r.

  @param  r      Resource to set attribute on
  @param  attr   Name of the Resource Attribute to set
  @param  value  Value to set the \a attr attribute to

  @returns 0 if the attribute value was set, or non-zero
           on failure.  Failure scenarios include problems
           parsing the value, invalid attributes, etc.
 */
int resource_set(struct resource *r, const char *attr, const char *value);

/**
  Query actual Resource State

  The name comes from the world of filesystems, where you "stat"
  a file to get its attributes.  Really, to "stat" a resource
  means to figure out what its real state is, and also determine
  what attributes are enforced but non-compliant.

  @note This function calls the res_*_stat resource callback
        appropriate to the type of resource \a r.

  @param  r    Resource to query state for
  @param  env  Resource Environment that may contain additional
               context for determining the state of the resource.

  @return 0 on success, non-zero on failure.  Specifically, if a
          resource is found not to exist locally, that is not
          considered failure.
 */
int resource_stat(struct resource *r, const struct resource_env *env);
/**
  Resource Fixup

  Bring the local resource (client-side) into compliance with
  the enforced attributes of the policy (server-side) definition.

  @note This function calls the res_*_fixup resource callback
        appropriate to the type of resource \a r.

  @param  r       Resource to "fix up"
  @param  dryrun  Don't do anything, just print what would be done
  @param  env     Resource Enfironment containing additional context
                  necessary to bring the resource into compliance.

  @returns a pointer to a struct report describing the actions taken,
           or NULL on internal failure.  Note that the inability to
           enforce the policy is not considered a failure.
 */
struct report* resource_fixup(struct resource *r, int dryrun, const struct resource_env *env);

/**
  Notify a Resource of a Dependency Change

  If resource a depends on resource b, and resource b changes
  (for compliance reasons), resource_notify should be used to let
  resource a know that something has changed.

  @note This function calls the res_*_notify resource callback
        appropriate to the type of resource \a r.

  @param  r     Resource to notify
  @param  dep   Causal resource (on which \a r depends) that changed

  @returns 0 on success, non-zero on failure.
 */
int resource_notify(struct resource *r, const struct resource *dep);

/**
  Serialize a Resource

  Using the pack routine from pack.c, serialize the data contained in
  the resource definition \a r into a string that can be stored or
  transmitted for use some other place or some other time.

  @note The string returned by this function should be freed by the
        caller, using the free(3) standard library function.

  The final serialized form of a resource can be passed to
  resource_unpack to get an identical in-memory resource structure
  back.

  @param  r   Resource to serialize

  @returns a heap-allocated string containing the packed
           representation of \a r, or NULL on failure.
 */
char *resource_pack(const struct resource *r);

/**
  Unserialize a Resource

  Using the unpack routine from pack.c, unserialize the string
  representation given and populate a resource definition containing
  the correct enforcement and attribute values.

  @note The pointer returned by this function should be passed to
        resource_free to de-allocate and release its memory.

  @param  packed   Packed (serialized) resource representation

  @returns a heap-allocated resource structure, based on the
           information found in the \a packed string.
 */
struct resource *resource_unpack(const char *packed);

/**
  Add a Resource Dependency

  Mainly intended for use by the Policy implementation, this
  function appends a dependent resource (\a dep) to another
  resources (\a r) list of dependencies.

  Policy uses this list later (via resource_depends_on and
  resource_drop_dependency) to re-order its list of resources
  so that it performs fixups in the appropriate order.

  @param  r    Dependent resource
  @param  dep  Causal resource, that \a r depends on.

  @returns 0 on success, non-zero on failure.
 */
int resource_add_dependency(struct resource *r, struct resource *dep);

/**
  Drop a Resource Dependency, by removing it (\a dep) from the
  list of another resources (\a r) dependencies.

  This function is used by Policy to indicate that further
  dependency tracking is not necessary.

  @param  r    Dependent resource
  @param  dep  Causal resource to remove from the dependency list

  @returns 0 on success, non-zero on failure.
 */
int resource_drop_dependency(struct resource *r, struct resource *dep);

/**
  Determine if Resource \a r depends on \a dep.

  Policy uses this function when it re-orders its list of
  resources into the appropriate sequence to satisfy all
  dependencies.

  @param  r    Dependent resource
  @param  dep  Causal resource to query

  @returns 0 is \a r depends on \a dep, non-zero otherwise.
 */
int resource_depends_on(const struct resource *r, const struct resource *dep);

/**
  Determine if a Resource matches an attribute filter.

  If this resource has an \a attr with a value of \a value,
  the resource is considered a match to the filter, and 0 is
  returned.

  @note This function calls the res_*_match resource callback
        appropriate to the type of resource \a r.

  @param  r      Resource to match
  @param  attr   Attribute to filter against
  @param  value  Value to look for

  @returns 0 is \a r matches the filter, non-zero otherwise.
 */
int resource_match(const struct resource *r, const char *attr, const char *value);

/**
  Allocate and initialize a new Dependency

  This function is used primarily by the spec parser.  It does not
  populate the \a resource_a or \a resource_b structure members,
  since there is not enough context to resolve resource identifiers.

  @note The pointer returned by this function should be passed to
        dependency_free to de-allocate and release its memory.

  @param  a  Fully-qualified resource identifier
             of the causal resource
  @param  b  Fully-qualified resource identifier
             of the dependent resource

  @returns a dynamically-allocated dependency structure, properly
           initialized, or NULL on failure.
 */
struct dependency* dependency_new(const char *a, const char *b);

/**
  Free a Dependency object

  @param  dep   Dependency object to free
 */
void dependency_free(struct dependency *dep);

/**
  Serialize a Dependency

  Using the pack routine from pack.c, serialize the data contained
  in the dependency definition \a dep into a string that can be stored
  or transmitted for use some other place or some other time.

  @note The string returned by this function should be freed by the
        caller, using the free(3) standard library function.

  The final serialized form of a dependency can be passed to
  dependency_unpack to get an identical in-memory dependency
  structure back.

  @param  dep   Dependency to serialize

  @returns a heap-allocated string containing the packed
           representation of \a d, or NULL on failure.
 */
char *dependency_pack(const struct dependency *dep);

/**
  Unserialize a Dependency

  Using the unpack routine from pack.c, unserialize the string
  representation given and populate a dependency definition
  containing the resource identifiers of the dependent and causal
  resources.

  This function does not resolve resource identifiers to their
  corresponding resources, since it lacks the context to do so.

  @note The pointer returned by this function should be passed to
        dependency_free to de-allocate and release its memory.

  @param  packed   Packed (serialized) dependency representation

  @returns a heap-allocatd dependency structure, based on the
           information found in the \a packed string.
 */
struct dependency *dependency_unpack(const char *packed);

#endif
