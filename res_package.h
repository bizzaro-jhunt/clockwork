#ifndef RES_PACKAGE_H
#define RES_PACKAGE_H

#include "clockwork.h"
#include "list.h"
#include "pkgmgr.h"

#define RES_PACKAGE_ABSENT   0x80000000
#define RES_PACKAGE_NONE     0x0

#define res_package_enforced(rp, field) (((rp)->enforced & RES_PACKAGE_ ## field) == RES_PACKAGE_ ## field)

struct res_package {
	char *key;
	char *name;
	char *version;

	char *installed;

	unsigned int enforced;
	unsigned int different;
	struct list res;
};

struct res_package* res_package_new(const char *key);
void res_package_free(struct res_package *rp);

int res_package_setattr(struct res_package *rp, const char *name, const char *value);

int res_package_set_presence(struct res_package *rp, int presence);
int res_package_set_name(struct res_package *rp, const char *name);
int res_package_set_version(struct res_package *rp, const char *version);

int res_package_stat(struct res_package *rp, const struct package_manager *mgr);
struct report* res_package_remediate(struct res_package *rp, int dryrun, const struct package_manager *mgr);

int res_package_is_pack(const char *packed);

char* res_package_pack(struct res_package *rp);
struct res_package* res_package_unpack(const char *packed);

#endif
