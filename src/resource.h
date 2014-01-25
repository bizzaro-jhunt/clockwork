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

#ifndef RESOURCE_H
#define RESOURCE_H

#include "clockwork.h"

#include <augeas.h>

#include "userdb.h"
#include "package.h"
#include "service.h"

/** Resource Type Identifiers */
enum restype {
	RES_USER,
	RES_GROUP,
	RES_FILE,
	RES_PACKAGE,
	RES_SERVICE,
	RES_HOST,
	RES_SYSCTL,
	RES_DIR,
	RES_EXEC,
	RES_UNKNOWN /* must be LAST */
};

/**
  Resource Environment

  Represents the full environment needed by all resources for fixups.

  For example, when fixing up user resources, res_user needs access to
  the password and shadow databases to create, update and remove accounts.
  These databases (pwdb and spdb structures) are stored in the resource
  environment and shared with all resources.

  As new resources are implemented, the resource_env structure will
  have to be updated with new members.
 */
struct resource_env {
	const char *package_manager;
	const char *service_manager;

	struct pwdb *user_pwdb;  /* /etc/passwd, for res_user */
	struct spdb *user_spdb;  /* /etc/shadow, for res_user */

	struct grdb *group_grdb; /* /etc/group,   for res_group */
	struct sgdb *group_sgdb; /* /etc/gshadow, for res_group */

	augeas *aug_context;     /* Augeas, for sub-file config edits */

	int file_fd;      /* used by res_file to refresh file contents. */
	ssize_t file_len; /* bytes to read from file_fd before EOF */
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
	enum restype type;   /* type of resource */

	void *resource;      /* pointer to the res_* structure for
	                        this resource; callback functions
	                        must cast this pointer as needed. */

	char *key;           /* fully-qualified identifier */

	struct resource **deps;  /* other resources this one depends on */
	int ndeps;               /* how many dependencies are there? */

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
  ensure that the user 'bob' exists before attempting to `chown` the
  file.  Implicit dependencies are created internally during the
  resource normalization process.

  Member names are based on a simple rule: a depends on b.
 */
struct dependency {
	char *a; /* id of dependent resource */
	char *b; /* id of resource that $a depends on */

	struct resource *resource_a; /* dependent resource */
	struct resource *resource_b; /* resource that $a depends on */

	struct list l;
};

/**
  CHeck if attribute $a is enforced on $r.

  Returns non-zero (true) if $a is enfored, and
  zero (false) if not.
 */
#define ENFORCED(r,a)  (((r)->enforced  & a) == a)

/**
  Check if attribute $a is out of compliance on $r.

  Returns non-zero (true) if $a is non-compliant, and
  zero (false) if not.
 */
#define DIFFERENT(r,a) (((r)->different & a) == a)

enum restype resource_type(const char *type_name);
struct resource* resource_new(const char *type, const char *key);
struct resource* resource_clone(const struct resource *orig, const char *key);
void resource_free(struct resource *r);
char *resource_key(const struct resource *r);
struct hash* resource_attrs(const struct resource *r);
int resource_norm(struct resource *r, struct policy *pol, struct hash *facts);
int resource_set(struct resource *r, const char *attr, const char *value);
int resource_stat(struct resource *r, const struct resource_env *env);
struct report* resource_fixup(struct resource *r, int dryrun, const struct resource_env *env);
int resource_notify(struct resource *r, const struct resource *dep);
char *resource_pack(const struct resource *r);
struct resource *resource_unpack(const char *packed);
int resource_match(const struct resource *r, const char *attr, const char *value);

int resource_add_dependency(struct resource *r, struct resource *dep);
int resource_drop_dependency(struct resource *r, struct resource *dep);
int resource_depends_on(const struct resource *r, const struct resource *dep);

struct dependency* dependency_new(const char *a, const char *b);
void dependency_free(struct dependency *dep);
char *dependency_pack(const struct dependency *dep);
struct dependency *dependency_unpack(const char *packed);

#endif
