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

static int NEXT_SERIAL = 1;

typedef void* (*resource_new_f)(const char *key);
typedef void* (*resource_clone_f)(const void *res, const char *key);
typedef void (*resource_free_f)(void *res);
typedef char* (*resource_key_f)(const void *res);
typedef int (*resource_attrs_f)(const void *res, hash_t *attrs);
typedef int (*resource_norm_f)(void *res, struct policy *pol, hash_t *facts);
typedef int (*resource_set_f)(void *res, const char *attr, const char *value);
typedef int (*resource_match_f)(const void *res, const char *attr, const char *value);
typedef int (*resource_gencode_f)(const void *res, FILE *io, unsigned int next, unsigned int serial);
typedef int (*resource_gencode2_f)(const void *res, FILE *io);
typedef FILE* (*resource_content_f)(const void *res, hash_t *facts);

#define RESOURCE_TYPE(t) { \
	             .name = #t,                    \
	     .new_callback = res_ ## t ## _new,     \
	   .clone_callback = res_ ## t ## _clone,   \
	    .free_callback = res_ ## t ## _free,    \
	     .key_callback = res_ ## t ## _key,     \
	   .attrs_callback = res_ ## t ## _attrs,   \
	    .norm_callback = res_ ## t ## _norm,    \
	     .set_callback = res_ ## t ## _set,     \
	   .match_callback = res_ ## t ## _match,   \
	 .gencode_callback = res_ ## t ## _gencode, \
	.gencode2_callback = res_ ## t ## _gencode2, \
	 .content_callback = res_ ## t ## _content, }

	const struct {
		const char         *name;

		resource_new_f      new_callback;
		resource_clone_f    clone_callback;
		resource_free_f     free_callback;
		resource_key_f      key_callback;
		resource_attrs_f    attrs_callback;
		resource_norm_f     norm_callback;
		resource_set_f      set_callback;
		resource_match_f    match_callback;
		resource_gencode_f  gencode_callback;
		resource_gencode2_f  gencode2_callback;
		resource_content_f  content_callback;

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
		RESOURCE_TYPE(symlink),
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

	struct resource *r = vmalloc(sizeof(struct resource));

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
	r->serial = NEXT_SERIAL++;
	return r;
}

struct resource* resource_clone(const struct resource *orig, const char *key)
{
	if (!orig || orig->type == RES_UNKNOWN) {
		return NULL;
	}

	struct resource *r = vmalloc(sizeof(struct resource));
	list_init(&r->l);
	r->type = orig->type;

	r->resource = (*(resource_types[r->type].clone_callback))(orig->resource, key);
	r->key = (*(resource_types[r->type].key_callback))(r->resource);
	r->ndeps = 0;
	r->deps = NULL;
	r->serial = NEXT_SERIAL++;
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

hash_t* resource_attrs(const struct resource *r)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	hash_t *attrs = vmalloc(sizeof(hash_t));
	if (!attrs) return NULL;

	if ((*(resource_types[r->type].attrs_callback))(r->resource, attrs) != 0) {
		hash_done(attrs, 1);
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
int resource_norm(struct resource *r, struct policy *pol, hash_t *facts)
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
				free(r->deps);
				r->deps = NULL;
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

int resource_gencode2(const struct resource *r, FILE *io)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].gencode2_callback))(r->resource, io);
}

int resource_gencode(const struct resource *r, FILE *io, unsigned int next)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].gencode_callback))(r->resource, io, next, r->serial);
}

FILE * resource_content(const struct resource *r, hash_t *facts)
{
	assert(r); // LCOV_EXCL_LINE
	assert(r->type != RES_UNKNOWN); // LCOV_EXCL_LINE

	return (*(resource_types[r->type].content_callback))(r->resource, facts);
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
	dep = vmalloc(sizeof(struct dependency));

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
		list_delete(&dep->l);
		free(dep->a);
		free(dep->b);
		free(dep);
	}
}
