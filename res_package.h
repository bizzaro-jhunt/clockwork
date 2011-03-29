#ifndef RES_PACKAGE_H
#define RES_PACKAGE_H

#include "clockwork.h"
#include "list.h"
#include "resource.h"

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

NEW_RESOURCE(package);

#endif
