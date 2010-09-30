#include <assert.h>
#include <string.h>
#include <errno.h>

#include "res_group.h"
#include "mem.h"

static int _res_group_diff(struct res_group *rg);
static int _res_group_stat_group(struct res_group *rg, struct grdb *db);
static int _res_group_stat_gshadow(struct res_group *rg, struct sgdb *db);

/*****************************************************************/

static int _res_group_diff(struct res_group *rg)
{
	assert(rg);

	rg->rg_diff = RES_GROUP_NONE;

	if (res_group_enforced(rg, NAME) && strcmp(rg->rg_name, rg->rg_grp.gr_name) != 0) {
		rg->rg_diff |= RES_GROUP_NAME;
	}

	if (res_group_enforced(rg, PASSWD) && strcmp(rg->rg_passwd, rg->rg_sg.sg_passwd) != 0) {
		rg->rg_diff |= RES_GROUP_PASSWD;
	}

	if (res_group_enforced(rg, GID) && rg->rg_gid != rg->rg_grp.gr_gid) {
		rg->rg_diff |= RES_GROUP_GID;
	}

	return 0;
}

static int _res_group_stat_group(struct res_group *rg, struct grdb *db)
{
	assert(rg);
	assert(rg->rg_name);
	assert(db);

	struct group *gr;

	gr = grdb_get_by_name(db, rg->rg_name);
	if (!gr) { return -1; }

	/* gr may point to static storage cf. getgrnam(3); */
	/* FIXME: this doesn't deep-copy string pointers in group structure */
	memcpy(&rg->rg_grp, gr, sizeof(struct group));
	return 0;
}

static int _res_group_stat_gshadow(struct res_group *rg, struct sgdb *db)
{
	assert(rg);
	assert(rg->rg_name);
	assert(db);

	struct sgrp *sg;

	sg = sgdb_get_by_name(db, rg->rg_name);
	if (!sg) { return -1; }

	/* sg may point to static storage */
	/* FIXME: this doesn't deep-copy string pointers in sgrp structure */
	memcpy(&rg->rg_sg, sg, sizeof(struct sgrp));
	return 0;
}

/*****************************************************************/

void res_group_init(struct res_group *rg)
{
	rg->rg_prio = 0;

	rg->rg_name = NULL;
	rg->rg_passwd = NULL;
	rg->rg_gid = 0;

	memset(&rg->rg_grp, 0, sizeof(struct group));
	memset(&rg->rg_sg,  0, sizeof(struct sgrp));

	rg->rg_enf = RES_GROUP_NONE;
	rg->rg_diff = RES_GROUP_NONE;
}

void res_group_free(struct res_group *rg)
{
	xfree(rg->rg_name);
	xfree(rg->rg_passwd);
}

int res_group_set_name(struct res_group *rg, const char *name)
{
	assert(rg);

	xfree(rg->rg_name);
	rg->rg_name = strdup(name);

	rg->rg_enf |= RES_GROUP_NAME;
	return 0;
}

int res_group_unset_name(struct res_group *rg)
{
	assert(rg);

	rg->rg_enf ^= RES_GROUP_NAME;
	return 0;
}

int res_group_set_passwd(struct res_group *rg, const char *passwd)
{
	assert(rg);

	xfree(rg->rg_passwd);
	rg->rg_passwd = strdup(passwd);
	if (!rg->rg_passwd) { return -1; }

	rg->rg_enf |= RES_GROUP_PASSWD;
	return 0;
}

int res_group_unset_passwd(struct res_group *rg)
{
	assert(rg);

	rg->rg_enf ^= RES_GROUP_PASSWD;
	return 0;
}

int res_group_set_gid(struct res_group *rg, gid_t gid)
{
	assert(rg);

	rg->rg_gid = gid;

	rg->rg_enf |= RES_GROUP_GID;
	return 0;
}

int res_group_unset_gid(struct res_group *rg)
{
	assert(rg);

	rg->rg_enf ^= RES_GROUP_GID;
	return 0;
}

void res_group_merge(struct res_group *rg1, struct res_group *rg2)
{
	assert(rg1);
	assert(rg2);

	struct res_group *tmp;

	if (rg1->rg_prio > rg2->rg_prio) {
		/* out-of-order, swap pointers */
		tmp = rg1; rg1 = rg2; rg2 = tmp; tmp = NULL;
	}

	/* rg1 has priority over rg2 */
	assert(rg1->rg_prio <= rg2->rg_prio);

	if ( res_group_enforced(rg2, NAME) &&
	    !res_group_enforced(rg1, NAME)) {
		res_group_set_name(rg1, rg2->rg_name);
	}

	if ( res_group_enforced(rg2, PASSWD) &&
	    !res_group_enforced(rg1, PASSWD)) {
		res_group_set_passwd(rg1, rg2->rg_passwd);
	}

	if ( res_group_enforced(rg2, GID) &&
	    !res_group_enforced(rg1, GID)) {
		res_group_set_gid(rg1, rg2->rg_gid);
	}
}

int res_group_stat(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb)
{
	assert(rg);
	assert(grdb);
	assert(sgdb);

	if (_res_group_stat_group(rg, grdb)   != 0) { return -1; }
	if (_res_group_stat_gshadow(rg, sgdb) != 0) { return -1; }

	return _res_group_diff(rg);
}
