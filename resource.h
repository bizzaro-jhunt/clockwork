#ifndef RESOURCE_H
#define RESOURCE_H

#include "clockwork.h"
#include "list.h"
#include "userdb.h"

#include "managers/package.h"
#include "managers/service.h"

/** @file resource.h

  These are some notes taken from res_file:


  Allocate and initialize a new res_file structure.

  The pointer returned must be freed by a call to res_file_free.

  @param  key    Key for new file resource.

  @returns a pointer to a heap-allocated res_file structure, with all
           members properly initialized to sane default values, or
           NULL on failure.
  void* res_TYPENAME_new(const char *key);

  Free dynamically allocated resource structure, \a r.
  void res_TYPENAME_free(void *r);

  Set an attribute on a resource.

  @param  r        Resource to set attribute on.
  @param  name     Name of attribute to set.
  @param  value    Value to set.
  int res_TYPENAME_set(void *r, const char *name, const char *value);

  Query the local filesystem and and populate the res_file::rf_stat
  member with information necessary for remediation.

  @param  rf    File resource to stat.

  @returns 0 on success, non-zero on failure.
  int res_TYPENAME_stat(void *r);

  Bring the local resource (client-side) into compliance with the
  enforced attributes of the server-side definition.

  @param  r       Resource to "fix up"
  @param  dryrun  Don't remediate, just print what would be done.
  @param  env     Resource Environment for performing fixups.

  @returns a pointer to a struct report describing actions taken,
           or NULL on internal failure (i.e. malloc issues)
  struct report *res_TYPENAME_fixup(void *r, int dryrun, struct resource_env *env);

  Serialize a resource into its packed string representation.

  The string returned by this function should be freed by the caller,
  using the standard free(3) standard library function.

  The final serialized form of a resource can be passed to
  res_TYPENAME_unpack to get the original structure back

  @param  r    Resource to serialize.

  @returns a heap-allocated string containing the packed representation
           of the given resource structure.
  char* res_file_pack(void *r);

  Unserialize a resource from its string representation.

  @note The pointer returned by this function should be passed to
        res_TYPENAME_free to de-allocate and release its memory resources.

  @param  packed    Packed (serialized) resource representation.

  @returns a heap-allocated resource structure, based on the information
           found in the \a packed string.
  void* res_file_unpack(const char *packed);
 */

enum restype {
	RES_USER,
	RES_GROUP,
	RES_FILE,
	RES_PACKAGE,
	RES_SERVICE,
	RES_UNKNOWN /* must be the LAST enumerated value */
};

struct resource_env {
	/* contains stuff needed by remediation routines */

	/* Used by res_package for native package management */
	const struct package_manager *package_manager;
	/* Used by res_service for native service management */
	const struct service_manager *service_manager;

	/* Used by res_user for /etc/passwd and /etc/shadow */
	struct pwdb *user_pwdb;
	struct spdb *user_spdb;

	/* Used by res_group for /etc/group and /etc/gshadow */
	struct grdb *group_grdb;
	struct sgdb *group_sgdb;

	/* Used by res_file to refresh local files from remote copies */
	int file_fd;
	ssize_t file_len;
};

struct policy;

struct resource {
	enum restype type;
	void *resource;
	const char *key;

	/** Other resources that this resource depends on. */
	struct resource **deps;
	/** Number of dependencies. */
	int ndeps;

	struct list l;
};

struct dependency {
	/* a depends on b */
	char *a, *b;

	struct resource *resource_a;
	struct resource *resource_b;

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

struct resource* resource_new(const char *type, const char *key);
void resource_free(struct resource *r);
const char *resource_key(const struct resource *r);
int resource_norm(struct resource *r, struct policy *pol);
int resource_set(struct resource *r, const char *attr, const char *value);
int resource_stat(struct resource *r, const struct resource_env *env);
struct report* resource_fixup(struct resource *r, int dryrun, const struct resource_env *env);
int resource_notify(struct resource *r, const struct resource *dep);
char *resource_pack(const struct resource *r);
struct resource *resource_unpack(const char *packed);
int resource_add_dependency(struct resource *r, struct resource *dep);
int resource_drop_dependency(struct resource *r, struct resource *dep);
int resource_depends_on(const struct resource *r, const struct resource *dep);
int resource_match(const struct resource *r, const char *attr, const char *value);

struct dependency* dependency_new(const char *a, const char *b);
void dependency_free(struct dependency *dep);
char *dependency_pack(const struct dependency *dep);
struct dependency *dependency_unpack(const char *packed);

#endif
