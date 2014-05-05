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

#include "resource.h"
#include "resources.h"

typedef void* (*resource_new_f)(const char *key);
typedef void* (*resource_clone_f)(const void *res, const char *key);
typedef void (*resource_free_f)(void *res);
typedef char* (*resource_key_f)(const void *res);
typedef int (*resource_attrs_f)(const void *res, struct hash *attrs);
typedef int (*resource_norm_f)(void *res, struct policy *pol, struct hash *facts);
typedef int (*resource_set_f)(void *res, const char *attr, const char *value);
typedef int (*resource_match_f)(const void *res, const char *attr, const char *value);
typedef int (*resource_gencode_f)(const void *res, FILE *io, unsigned int next);
typedef int (*resource_stat_f)(void *res, const struct resource_env *env);
typedef struct report* (*resource_fixup_f)(void *res, int dryrun, const struct resource_env *env);
typedef int (*resource_notify_f)(void *res, const struct resource *dep);
typedef char* (*resource_pack_f)(const void *res);
typedef void* (*resource_unpack_f)(const char *packed);

#define RESOURCE_TYPE(t) { \
	             .name = #t,                    \
	      .pack_prefix = "res_" #t "::",        \
	     .new_callback = res_ ## t ## _new,     \
	   .clone_callback = res_ ## t ## _clone,   \
	    .free_callback = res_ ## t ## _free,    \
	     .key_callback = res_ ## t ## _key,     \
	   .attrs_callback = res_ ## t ## _attrs,   \
	    .norm_callback = res_ ## t ## _norm,    \
	     .set_callback = res_ ## t ## _set,     \
	   .match_callback = res_ ## t ## _match,   \
	 .gencode_callback = res_ ## t ## _gencode, \
	    .stat_callback = res_ ## t ## _stat,    \
	   .fixup_callback = res_ ## t ## _fixup,   \
	  .notify_callback = res_ ## t ## _notify,  \
	    .pack_callback = res_ ## t ## _pack,    \
	  .unpack_callback = res_ ## t ## _unpack   }

	const struct {
		const char         *name;
		const char         *pack_prefix;

		resource_new_f      new_callback;
		resource_clone_f    clone_callback;
		resource_free_f     free_callback;
		resource_key_f      key_callback;
		resource_attrs_f    attrs_callback;
		resource_norm_f     norm_callback;
		resource_set_f      set_callback;
		resource_match_f    match_callback;
		resource_gencode_f  gencode_callback;
		resource_stat_f     stat_callback;
		resource_fixup_f    fixup_callback;
		resource_notify_f   notify_callback;
		resource_pack_f     pack_callback;
		resource_unpack_f   unpack_callback;

	} resource_types[RES_UNKNOWN] = {
		RESOURCE_TYPE(user),
		RESOURCE_TYPE(group),
		RESOURCE_TYPE(file),
		RESOURCE_TYPE(package),
		RESOURCE_TYPE(service),
		RESOURCE_TYPE(host),
		RESOURCE_TYPE(sysctl),
		RESOURCE_TYPE(dir),
		RESOURCE_TYPE(exec),
	};

#undef RESOURCE_TYPE

enum restype resource_type(const char *name)
{
	assert(name);

	enum restype type;
	for (type = 0; type < RES_UNKNOWN; type++) {
		if (strcmp(resource_types[type].name, name) == 0) {
			return type;
		}
	}
	return RES_UNKNOWN;
}

/**
  Create a new $type resource.

  Since this function is primarily used by the spec parser,
  $type is given as a resource type name string, like
  "user" or "service".  $key is the unqualified identifier
  for the new resource.  $key can be NULL, for default
  resources.

  This function calls the `res_*_new` resource callback
  appropriate to the type of resource to be created.

  The pointer returned must be freed by a call to @resource_free.

  On success, returns a new resource, initialized with sane
  default values.
  On failure, returns NULL.
 */
struct resource* resource_new(const char *type, const char *key)
{
	assert(type); // LCOV_EXCL_LINE

	struct resource *r = xmalloc(sizeof(struct resource));

	list_init(&r->l);
	r->key = NULL;
	r->type = resource_type(type);
	if (r->type == RES_UNKNOWN) {
		free(r);
		return NULL;
	}
	r->resource = (*(resource_types[r->type].new_callback))(key);
	r->key = (*(resource_types[r->type].key_callback))(r->resource);
	r->ndeps = 0;
	r->deps = NULL;
	return r;
}

struct resource* resource_clone(const struct resource *orig, const char *key)
{
	if (!orig || orig->type == RES_UNKNOWN) {
		return NULL;
	}

	struct resource *r = xmalloc(sizeof(struct resource));
	list_init(&r->l);
	r->type = orig->type;

	r->resource = (*(resource_types[r->type].clone_callback))(orig->resource, key);
	r->key = (*(resource_types[r->type].key_callback))(r->resource);
	r->ndeps = 0;
	r->deps = NULL;
	return r;
}

/**
  Free resource $r.

  This function calls the `res_*_free` resource callback
  appropriate to the type of resource $r.
 */
void resource_free(struct resource *r)
{
	if (r) {
		(*(resource_types[r->type].free_callback))(r->resource);
		free(r->key);
	}
	free(r);
}

/**
  Fully-Qualified Resource Identifier

  This function generates a fully-qualified resource identifier
  for $r, based on its type and unqualified identifier.

  The string pointer returned must be freed by the caller.

  On success, returns a string.  On failure, returns NULL.
 */
char *resource_key(const struct resource *r)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].key_callback))(r->resource);
}

struct hash* resource_attrs(const struct resource *r)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	struct hash *attrs = hash_new();
	if (!attrs) { return NULL; }

	if ((*(resource_types[r->type].attrs_callback))(r->resource, attrs) != 0) {
		hash_free_all(attrs);
		return NULL;
	}

	return attrs;
}

/**
  Normalize a Resource

  Some resources need additional processing beyond setting of
  attribute values.  This function provides implementors a
  hook into the policy parsing / reconstitution process so that
  these normalizations can be performed before the resource
  goes into "fix up" mode.

  This function calls the `res_*_norm resource` callback
  appropriate to the type of resource $r.

  The $pol parameter allows the resource implementation to
  locate other resources for implicit dependency injection.
  It is assumed that by the time `resource_norm` is called, all
  resource definitions have been added to the policy.

  On success, returns 0.  On failure, returns non-zero.
 */
int resource_norm(struct resource *r, struct policy *pol, struct hash *facts)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].norm_callback))(r->resource, pol, facts);
}

/**
  Set $attr to $value on $r.

  This function calls the res_*_set resource callback
  appropriate to the type of resource $r.

  If the attribute was properly set, returns 0 to indicate success.

  On failure, returns non-zero.  Failure scenarios include problems
  parsing the value, invalid attributes, etc.
 */
int resource_set(struct resource *r, const char *attr, const char *value)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE
	assert(attr); // LCOV_EXCL_LINE
	assert(value); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].set_callback))(r->resource, attr, value);
}

/**
  Query actual state of $r

  The name comes from the world of filesystems, where you "stat"
  a file to get its attributes.  Really, to "stat" a resource
  means to figure out what its real state is, and also determine
  what attributes are enforced but non-compliant.

  This function calls the `res_*_stat` resource callback
  appropriate to the type of resource $r.

  On success, returns 0.  On failure, returns non-zero.  Specifically,
  if a resource is found not to exist locally, that s not considered
  a failure.
 */
int resource_stat(struct resource *r, const struct resource_env *env)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].stat_callback))(r->resource, env);
}

/**
  Fixup resource $r.

  Bring the local resource (client-side) into compliance with
  the enforced attributes of the policy (server-side) definition.

  This function calls the `res_*_fixup` resource callback
  appropriate to the type of resource $r.

  If $dryrun is non-zero, no actions will be taken against the system,
  but the returned report will still describe the actions that would
  have been taken, each with a result of 'skipped.'

  On success, returns a report that describes what actions were taken
  and what their results were.  On failure, return NULL.

  The inability to enforce the policy is *not* considered a failure.
 */
struct report* resource_fixup(struct resource *r, int dryrun, const struct resource_env *env)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].fixup_callback))(r->resource, dryrun, env);
}

/**
  Notify $r of a Dependency Change

  If resource a depends on resource b, and resource b changes
  (for compliance reasons), `resource_notify` us used to let
  resource a know that something has changed.

  $dep is the causal resource (the one that $r depends on) that
  changed.

  This function calls the res_*_notify resource callback
  appropriate to the type of resource $r.

  On success, returns 0.  On failure, returns non-zero.
 */
int resource_notify(struct resource *r, const struct resource *dep)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].notify_callback))(r->resource, dep);
}

/**
  Pack resource $r.

  Using @pack, serialize the data contained in $r into a string
  that can be stored or transmitted for use some other place or
  some other time.

  The string returned by this function should be freed by the caller.

  The final serialized form of a resource can be passed to
  @resource_unpack to get an identical in-memory resource structure
  back.

  On success, returns the string representation of $r.
  On failure, returns NULL.
 */
char *resource_pack(const struct resource *r)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].pack_callback))(r->resource);
}

/**
  Unpack $packed into a resource.

  Using @unpack, unserialize the string representation given
  and populate a resource definition containing the correct
  enforcement and attribute values.

  The pointer returned by this function should be passed to
  @resource_free to de-allocate and release its memory.

  On success, returns the resource that $packed represents.
  On failure, returns NULL.
 */
struct resource *resource_unpack(const char *packed)
{
	assert(packed); // LCOV_EXCL_LINE
	struct resource *r = NULL;

	/* This one is a bit different, because we need to determine
	   the resource data type from the packed prefix. */

	enum restype i;
	for (i = 0; i < RES_UNKNOWN; i++) {
		if (strncmp(packed, resource_types[i].pack_prefix,
		            strlen(resource_types[i].pack_prefix)) == 0) {
			r = xmalloc(sizeof(struct resource));
			r->type = i;
			r->resource = (*(resource_types[i].unpack_callback))(packed);
			r->key = (*(resource_types[i].key_callback))(r->resource);
			break;
		}
	}

	return r;
}

/**
  Add a Resource Dependency

  Mainly intended for use by the Policy implementation, this
  function appends a dependent resource ($dep) to another
  resource's ($r) list of dependencies.

  Policy uses this list later (via @resource_depends_on and
  @resource_drop_dependency) to re-order its list of resources
  so that it performs fixups in the appropriate order.

  On success, returns 0.  On failure, returns non-zero.
 */
int resource_add_dependency(struct resource *r, struct resource *dep)
{
	assert(r); // LCOV_EXCL_LINE
	assert(dep); // LCOV_EXCL_LINE

	r->deps = realloc(r->deps, sizeof(struct resource*) * (r->ndeps+1));
	r->deps[r->ndeps++] = dep;
	return 0;
}

/**
  Drop a Resource Dependency.

  This function erases a dependency, by removing $dep from the
  list of $r's dependencies.

  This function is used by Policy to indicate that further
  dependency tracking is not necessary.

  On success, returns 0.  On failure, returns non-zero.
 */
int resource_drop_dependency(struct resource *r, struct resource *dep)
{
	assert(r); // LCOV_EXCL_LINE
	assert(dep); // LCOV_EXCL_LINE

	int i, j;
	for (i = 0; i < r->ndeps; i++) {
		if (r->deps[i] == dep) {
			for (j = i+1; j < r->ndeps; j++) {
				r->deps[i++] = r->deps[j];
			}
			r->ndeps--;
			if (r->ndeps == 0) {
				xfree(r->deps);
			}
			return 0;
		}
	}

	return 1; /* not found */
}

/**
  Determine if $r depends on $dep.

  Policy uses this function when it re-orders its list of
  resources into the appropriate sequence to satisfy all
  dependencies.

  If $r depends on $dep, returns 0.  If not, returns non-zero.
 */
int resource_depends_on(const struct resource *r, const struct resource *dep)
{
	int i;

	for (i = 0; i < r->ndeps; i++) {
		if (r->deps[i] == dep) { return 0; }
	}
	return 1; /* no dependency */
}

/**
  Check $r to see if $attr equals $value.

  This function calls the res_*_match resource callback
  appropriate to the type of resource $r.

  If $attr equals $value on $r, returns 0.  If not, returns non-zero.
 */
int resource_match(const struct resource *r, const char *attr, const char *value)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].match_callback))(r->resource, attr, value);
}

int resource_gencode(const struct resource *r, FILE *io, unsigned int next)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].gencode_callback))(r->resource, io, next);
}

/**
  Create a new Dependency of $a on $b.

  This function is used primarily by the spec parser.  It does not
  populate the $resource_a or $resource_b structure members,
  since there is not enough context to resolve resource identifiers.

  The pointer returned by this function should be passed to
  @dependency_free to de-allocate and release its memory.

  On success, returns a new dependency.  On failure, returns NULL.
 */
struct dependency* dependency_new(const char *a, const char *b)
{
	struct dependency *dep;

	dep = xmalloc(sizeof(struct dependency));

	if (a) { dep->a = strdup(a); }
	if (b) { dep->b = strdup(b); }
	list_init(&dep->l);

	return dep;
}

/**
  Free dependency $dep.
 */
void dependency_free(struct dependency *dep)
{
	if (dep) {
		list_del(&dep->l);
		free(dep->a);
		free(dep->b);
		free(dep);
	}
}

#define PACK_FORMAT "aa"
/**
  Pack Dependency $dep

  Using @pack, serialize $dep into a string that can be stored
  or transmitted for use some other place or some other time.

  The string returned by this function should be freed by the caller.

  The final serialized form of a dependency can be passed to
  @dependency_unpack to get an identical in-memory dependency
  structure back.

  On success, returns a string representing $dep.
  On failure, returns NULL.
 */
char *dependency_pack(const struct dependency *dep)
{
	assert(dep); // LCOV_EXCL_LINE

	return pack("dependency::", PACK_FORMAT, dep->a, dep->b);
}

/**
  Unserialize $packed into a Dependency

  Using @unpack, unserialize $packed and populate a dependency
  definition containing the resource identifiers of the dependent
  and causal resources.

  This function does not resolve resource identifiers to their
  corresponding resources, since it lacks the context to do so.

  The pointer returned by this function should be passed to
  @dependency_free to de-allocate and release its memory.

  On success, returns the dependency represented by $packed.
  On failure, returns NULL.
 */
struct dependency *dependency_unpack(const char *packed)
{
	struct dependency *dep = dependency_new(NULL, NULL);

	if (unpack(packed, "dependency::", PACK_FORMAT,
	    &dep->a, &dep->b) != 0) {

		dependency_free(dep);
		return NULL;
	}

	return dep;
}
#undef PACK_FORMAT
