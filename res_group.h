#ifndef RES_GROUP_H
#define RES_GROUP_H

#include <stdlib.h>
#include <sys/types.h>
#include <grp.h>

#include "list.h"
#include "userdb.h"
#include "stringlist.h"

#define RES_GROUP_ABSENT   0x80000000

#define RES_GROUP_NONE     0x00
#define RES_GROUP_NAME     0x01
#define RES_GROUP_PASSWD   0x02
#define RES_GROUP_GID      0x04
#define RES_GROUP_MEMBERS  0x08
#define RES_GROUP_ADMINS   0x10

#define res_group_enforced(rg, flag)  (((rg)->rg_enf  & RES_GROUP_ ## flag) == RES_GROUP_ ## flag)
#define res_group_different(rg, flag) (((rg)->rg_diff & RES_GROUP_ ## flag) == RES_GROUP_ ## flag)

struct res_group {
	char          *key;        /* Unique Identifier; starts with "res_group:" */

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

	unsigned int   rg_enf;     /* enforce-compliance flags */
	unsigned int   rg_diff;    /* out-of-compliance flags */

	struct list    res;       /* Node in policy list */
};

struct res_group *res_group_new(const char *key);
void res_group_free(struct res_group *rg);

int res_group_setattr(struct res_group *rg, const char *name, const char *value);

int res_group_set_presence(struct res_group *rg, int presence);

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

int res_group_stat(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb);
int res_group_remediate(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb);

int res_group_is_pack(const char *packed);
char *res_group_pack(struct res_group *rg);
struct res_group* res_group_unpack(const char *packed);

#endif
