#include "resource.h"
#include "resources.h"
#include "pack.h"

typedef void* (*resource_new_f)(const char *key);
typedef void (*resource_free_f)(void *res);
typedef char* (*resource_key_f)(const void *res);
typedef int (*resource_norm_f)(void *res, struct policy *pol, struct hash *facts);
typedef int (*resource_set_f)(void *res, const char *attr, const char *value);
typedef int (*resource_match_f)(const void *res, const char *attr, const char *value);
typedef int (*resource_stat_f)(void *res, const struct resource_env *env);
typedef struct report* (*resource_fixup_f)(void *res, int dryrun, const struct resource_env *env);
typedef int (*resource_notify_f)(void *res, const struct resource *dep);
typedef char* (*resource_pack_f)(const void *res);
typedef void* (*resource_unpack_f)(const char *packed);

#define RESOURCE_TYPE(t) { \
	             .name = #t,                    \
	      .pack_prefix = "res_" #t "::",        \
	     .new_callback = res_ ## t ## _new,     \
	    .free_callback = res_ ## t ## _free,    \
	     .key_callback = res_ ## t ## _key,     \
	    .norm_callback = res_ ## t ## _norm,    \
	     .set_callback = res_ ## t ## _set,     \
	   .match_callback = res_ ## t ## _match,   \
	    .stat_callback = res_ ## t ## _stat,    \
	   .fixup_callback = res_ ## t ## _fixup,   \
	  .notify_callback = res_ ## t ## _notify,  \
	    .pack_callback = res_ ## t ## _pack,    \
	  .unpack_callback = res_ ## t ## _unpack   }

	const struct {
		const char         *name;
		const char         *pack_prefix;

		resource_new_f      new_callback;
		resource_free_f     free_callback;
		resource_key_f      key_callback;
		resource_norm_f     norm_callback;
		resource_set_f      set_callback;
		resource_match_f    match_callback;
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
		RESOURCE_TYPE(host)
	};

#undef RESOURCE_TYPE

struct resource* resource_new(const char *type, const char *key)
{
	assert(type);
	assert(key);

	enum restype i;
	struct resource *r = xmalloc(sizeof(struct resource));

	list_init(&r->l);
	r->key = NULL;
	r->type = RES_UNKNOWN;
	for (i = 0; i < RES_UNKNOWN; i++) {
		if (strcmp(resource_types[i].name, type) == 0) {
			r->type = i;
		}
	}

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

void resource_free(struct resource *r)
{
	if (r) {
		(*(resource_types[r->type].free_callback))(r->resource);
		free(r->key);
	}
	free(r);
}

char *resource_key(const struct resource *r)
{
	assert(r);
	assert(r->type != RES_UNKNOWN);

	return (*(resource_types[r->type].key_callback))(r->resource);
}

int resource_norm(struct resource *r, struct policy *pol, struct hash *facts)
{
	assert(r);
	assert(r->type != RES_UNKNOWN);

	return (*(resource_types[r->type].norm_callback))(r->resource, pol, facts);
}

int resource_set(struct resource *r, const char *attr, const char *value)
{
	assert(r);
	assert(r->type != RES_UNKNOWN);
	assert(attr);
	assert(value);

	return (*(resource_types[r->type].set_callback))(r->resource, attr, value);
}

int resource_stat(struct resource *r, const struct resource_env *env)
{
	assert(r);
	assert(r->type != RES_UNKNOWN);
	assert(env);

	return (*(resource_types[r->type].stat_callback))(r->resource, env);
}

struct report* resource_fixup(struct resource *r, int dryrun, const struct resource_env *env)
{
	assert(r);
	assert(r->type != RES_UNKNOWN);
	assert(env);

	return (*(resource_types[r->type].fixup_callback))(r->resource, dryrun, env);
}

int resource_notify(struct resource *r, const struct resource *dep)
{
	assert(r);
	assert(r->type != RES_UNKNOWN);

	return (*(resource_types[r->type].notify_callback))(r->resource, dep);
}

char *resource_pack(const struct resource *r)
{
	assert(r);
	assert(r->type != RES_UNKNOWN);

	return (*(resource_types[r->type].pack_callback))(r->resource);
}

struct resource *resource_unpack(const char *packed)
{
	assert(packed);
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

int resource_add_dependency(struct resource *r, struct resource *dep)
{
	assert(r);
	assert(dep);

	r->deps = realloc(r->deps, sizeof(struct resource*) * (r->ndeps+1));
	r->deps[r->ndeps++] = dep;
	return 0;
}

int resource_drop_dependency(struct resource *r, struct resource *dep)
{
	assert(r);
	assert(dep);

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

int resource_depends_on(const struct resource *r, const struct resource *dep)
{
	int i;

	for (i = 0; i < r->ndeps; i++) {
		if (r->deps[i] == dep) { return 0; }
	}
	return 1; /* no dependency */
}

int resource_match(const struct resource *r, const char *attr, const char *value)
{
	assert(r);
	assert(r->type != RES_UNKNOWN);

	return (*(resource_types[r->type].match_callback))(r->resource, attr, value);
}

struct dependency* dependency_new(const char *a, const char *b)
{
	struct dependency *dep;

	dep = xmalloc(sizeof(struct dependency));

	if (a) { dep->a = strdup(a); }
	if (b) { dep->b = strdup(b); }
	list_init(&dep->l);

	return dep;
}

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
char *dependency_pack(const struct dependency *dep)
{
	assert(dep);

	return pack("dependency::", PACK_FORMAT, dep->a, dep->b);
}

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
