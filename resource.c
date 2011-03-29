#include "resource.h"

#include "res_user.h"
#include "res_group.h"
#include "res_file.h"
#include "res_package.h"

typedef void* (*resource_new_f)(const char *key);
typedef void (*resource_free_f)(void *res);
typedef const char* (*resource_key_f)(const void *res);
typedef int (*resource_norm_f)(void *res);
typedef int (*resource_set_f)(void *res, const char *attr, const char *value);
typedef int (*resource_stat_f)(void *res, const struct resource_env *env);
typedef struct report* (*resource_fixup_f)(void *res, int dryrun, const struct resource_env *env);
typedef int (*resource_is_pack_f)(const char *packed);
typedef char* (*resource_pack_f)(const void *res);
typedef void* (*resource_unpack_f)(const char *packed);

#define RESOURCE_TYPE(t) { \
	             .name = #t,                    \
	     .new_callback = res_ ## t ## _new,     \
	    .free_callback = res_ ## t ## _free,    \
	     .key_callback = res_ ## t ## _key,     \
	    .norm_callback = res_ ## t ## _norm,    \
	     .set_callback = res_ ## t ## _set,     \
	    .stat_callback = res_ ## t ## _stat,    \
	   .fixup_callback = res_ ## t ## _fixup,   \
	 .is_pack_callback = res_ ## t ## _is_pack, \
	    .pack_callback = res_ ## t ## _pack,    \
	  .unpack_callback = res_ ## t ## _unpack   }

	const struct {
		const char         *name;
		resource_new_f      new_callback;
		resource_free_f     free_callback;
		resource_key_f      key_callback;
		resource_norm_f     norm_callback;
		resource_set_f      set_callback;
		resource_stat_f     stat_callback;
		resource_fixup_f    fixup_callback;
		resource_is_pack_f  is_pack_callback;
		resource_pack_f     pack_callback;
		resource_unpack_f   unpack_callback;
	} resource_types[RES_UNKNOWN] = {
		RESOURCE_TYPE(user),
		RESOURCE_TYPE(group),
		RESOURCE_TYPE(file),
		RESOURCE_TYPE(package)
	};

#undef RESOURCE_TYPE

struct resource* resource_new(const char *type, const char *key)
{
	assert(type);
	assert(key);

	enum restype i;
	struct resource *r = xmalloc(sizeof(struct resource));

	r->key = NULL;
	r->type = RES_UNKNOWN;
	for (i = 0; i < RES_UNKNOWN; i++) {
		if (strcmp(resource_types[i].name, type) == 0) {
			r->type = i;
		}
	}

	if (r->type == RES_UNKNOWN) {
		/* FIXME: debug? */
		free(r);
		return NULL;
	}
	r->resource = (*(resource_types[r->type].new_callback))(key);
	r->key = (*(resource_types[r->type].key_callback))(r->resource);
	return r;
}

void resource_free(struct resource *r)
{
	if (r) {
		(*(resource_types[r->type].free_callback))(r->resource);
	}
	free(r);
}

int resource_norm(struct resource *r)
{
	assert(r);
	assert(r->type != RES_UNKNOWN);

	return (*(resource_types[r->type].norm_callback))(r->resource);
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
		if ( (*(resource_types[i].is_pack_callback))(packed) == 0 ) {
			r = xmalloc(sizeof(struct resource));
			r->type = i;
			r->resource = (*(resource_types[i].unpack_callback))(packed);
			r->key = (*(resource_types[i].key_callback))(r->resource);
			break;
		}
	}

	return r;
}

