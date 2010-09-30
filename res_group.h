#ifndef RES_GROUP_H
#define RES_GROUP_H

#include <stdlib.h>
#include <sys/types.h>
#include <grp.h>

#include "userdb.h"

#define RES_GROUP_NONE   0000
#define RES_GROUP_NAME   0001
#define RES_GROUP_PASSWD 0002
#define RES_GROUP_GID    0004

#define res_group_enforced(rg, flag)  (((rg)->rg_enf  & RES_GROUP_ ## flag) == RES_GROUP_ ## flag)
#define res_group_different(rg, flag) (((rg)->rg_diff & RES_GROUP_ ## flag) == RES_GROUP_ ## flag)

struct res_group {
	char          *rg_name;    /* Group name */
	char          *rg_passwd;  /* Group membership password (encrypted) */
	gid_t          rg_gid;     /* Numeric Group ID */

	struct group   rg_grp;     /* cf. getgrnam(3) */
	struct sgrp    rg_sg;      /* cf. /usr/include/sgrp.h */

	unsigned int   rg_prio;
	unsigned int   rg_enf;     /* enforce-compliance flags */
	unsigned int   rg_diff;    /* out-of-compliance flags */
};

void res_group_init(struct res_group *rg);
void res_group_free(struct res_group *rg);

int res_group_set_name(struct res_group *rg, const char *name);
int res_group_unset_name(struct res_group *rg);

int res_group_set_passwd(struct res_group *rg, const char *passwd);
int res_group_unset_passwd(struct res_group *rg);

int res_group_set_gid(struct res_group *rg, gid_t gid);
int res_group_unset_gid(struct res_group *rg);

void res_group_merge(struct res_group *rg1, struct res_group *rg2);

int res_group_stat(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb);

#endif
