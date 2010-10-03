#ifndef RES_GROUP_H
#define RES_GROUP_H

#include <stdlib.h>
#include <sys/types.h>
#include <grp.h>

#include "userdb.h"
#include "stringlist.h"

#define RES_GROUP_NONE          0
#define RES_GROUP_NAME     1 << 1
#define RES_GROUP_PASSWD   1 << 2
#define RES_GROUP_GID      1 << 3
#define RES_GROUP_MEMBERS  1 << 4
#define RES_GROUP_ADMINS   1 << 5

#define res_group_enforced(rg, flag)  (((rg)->rg_enf  & RES_GROUP_ ## flag) == RES_GROUP_ ## flag)
#define res_group_different(rg, flag) (((rg)->rg_diff & RES_GROUP_ ## flag) == RES_GROUP_ ## flag)

struct res_group {
	char          *rg_name;    /* Group name */
	char          *rg_passwd;  /* Group membership password (encrypted) */
	gid_t          rg_gid;     /* Numeric Group ID */

	stringlist    *rg_mem_add; /* Users that should belong to this group */
	stringlist    *rg_mem_rm;  /* Users that cannot belong to this group */
	stringlist    *rg_mem;     /* Copy of gr_mem, from stat, for remediation */

	stringlist    *rg_adm_add; /* Users that should manage this group */
	stringlist    *rg_adm_rm;  /* Users that cannot manage this group */
	stringlist    *rg_adm;     /* Copy of sg_adm, from stat, for remediation */

	struct group  *rg_grp;     /* pointer to an entry in grdb */
	struct sgrp   *rg_sg;      /* pointer to an entry in sgdb */

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

int res_group_enforce_members(struct res_group *rg, int enforce);
int res_group_add_member(struct res_group *rg, const char *user);
int res_group_remove_member(struct res_group *rg, const char *user);

int res_group_enforce_admins(struct res_group *rg, int enforce);
int res_group_add_admin(struct res_group *rg, const char *user);
int res_group_remove_admin(struct res_group *rg, const char *user);

void res_group_merge(struct res_group *rg1, struct res_group *rg2);

int res_group_stat(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb);
int res_group_remediate(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb);

#endif
