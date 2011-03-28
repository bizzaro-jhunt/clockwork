#ifndef RESOURCE_H
#define RESOURCE_H

#include "clockwork.h"
#include "list.h"
#include "pkgmgr.h"
#include "userdb.h"

enum restype {
	RES_USER,
	RES_GROUP,
	RES_FILE,
	RES_PACKAGE,
	RES_UNKNOWN /* must be the LAST enumerated value */
};

struct resource_env {
	/* contains stuff needed by remediation routines */

	/* Used by res_package for native package management */
	const struct package_manager *package_manager;

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

struct resource {
	enum restype type;
	void *resource;
	const char *key;

	struct list l;
};

#define NEW_RESOURCE(t) \
void*          res_ ## t ## _new(const char *key); \
void           res_ ## t ## _free(void *res); \
const char*    res_ ## t ## _key(const void *res); \
int            res_ ## t ## _norm(void *res); \
int            res_ ## t ## _setattr(void *res, const char *attr, const char *value); \
int            res_ ## t ## _stat(void *res, const struct resource_env *env); \
struct report* res_ ## t ## _fixup(void *res, int dryrun, const struct resource_env *env); \
int            res_ ## t ## _is_pack(const char *packed); \
char*          res_ ## t ## _pack(const void *res); \
void*          res_ ## t ## _unpack(const char *packed)

struct resource* resource_new(const char *type, const char *key);
void resource_free(struct resource *r);
int resource_norm(struct resource *r);
int resource_setattr(struct resource *r, const char *attr, const char *value);
int resource_stat(struct resource *r, const struct resource_env *env);
struct report* resource_fixup(struct resource *r, int dryrun, const struct resource_env *env);
char *resource_pack(const struct resource *r);
struct resource *resource_unpack(const char *packed);

#endif
