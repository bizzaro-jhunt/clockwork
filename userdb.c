#define _SVID_SOURCE
#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "userdb.h"
#include "mem.h"

/**********************************************************/

/* Allocate a userdb entry (pwdb, sgdb, etc.) and its payload
   member (passwd, sgrp, etc.), while handling errors.  Since
   this code was common to at four functions, it was macrofied. */
#define ALLOC_ENTRY(db_t,db,ent_t,ent) do {\
	if (!(db = malloc(sizeof(db_t)))) { return NULL; } \
	db->next = NULL; \
	if (!(ent = malloc(sizeof(ent_t)))) { \
		free(db); \
		return NULL; \
	} \
} while(0)

static struct pwdb* _pwdb_entry(struct passwd *passwd);
static struct pwdb* _pwdb_fgetpwent(FILE *input);
static void _passwd_free(struct passwd *passwd);
static void _pwdb_entry_free(struct pwdb *entry);

static struct spdb* _spdb_entry(struct spwd *spwd);
static void _shadow_free(struct spwd *spwd);
static void _spdb_entry_free(struct spdb *entry);

static struct grdb* _grdb_entry(struct group *group);
static struct grdb* _grdb_fgetgrent(FILE *input);
static void _group_free(struct group *group);
static void _grdb_entry_free(struct grdb *entry);

static struct sgdb* _sgdb_entry(struct sgrp *sgrp);
static void _gshadow_free(struct sgrp *sgrp);
static void _sgdb_entry_free(struct sgdb *entry);

/**********************************************************/

static struct pwdb* _pwdb_entry(struct passwd *passwd)
{
	assert(passwd);

	struct pwdb *ent;
	ALLOC_ENTRY(struct pwdb, ent, struct passwd, ent->passwd);

	ent->passwd->pw_name   = xstrdup(passwd->pw_name);
	ent->passwd->pw_passwd = xstrdup(passwd->pw_passwd);
	ent->passwd->pw_uid    = passwd->pw_uid;
	ent->passwd->pw_gid    = passwd->pw_gid;
	ent->passwd->pw_gecos  = xstrdup(passwd->pw_gecos);
	ent->passwd->pw_dir    = xstrdup(passwd->pw_dir);
	ent->passwd->pw_shell  = xstrdup(passwd->pw_shell);

	return ent;
}

static struct pwdb* _pwdb_fgetpwent(FILE *input)
{
	assert(input);

	struct passwd *passwd = fgetpwent(input);
	if (!passwd) { return NULL; }
	return _pwdb_entry(passwd);
}

static void _passwd_free(struct passwd *pw)
{
	if (!pw) { return; }
	xfree(pw->pw_name);
	xfree(pw->pw_passwd);
	xfree(pw->pw_gecos);
	xfree(pw->pw_dir);
	xfree(pw->pw_shell);
	xfree(pw);
}

static void _pwdb_entry_free(struct pwdb *entry)
{
	if (!entry) { return; }
	_passwd_free(entry->passwd);
	free(entry);
}

static struct spdb* _spdb_entry(struct spwd *spwd)
{
	assert(spwd);

	struct spdb *ent;
	ALLOC_ENTRY(struct spdb, ent, struct spwd, ent->spwd);

	ent->spwd->sp_namp   = xstrdup(spwd->sp_namp);
	ent->spwd->sp_pwdp   = xstrdup(spwd->sp_pwdp);
	ent->spwd->sp_lstchg = spwd->sp_lstchg;
	ent->spwd->sp_min    = spwd->sp_min;
	ent->spwd->sp_max    = spwd->sp_max;
	ent->spwd->sp_warn   = spwd->sp_warn;
	ent->spwd->sp_inact  = spwd->sp_inact;
	ent->spwd->sp_expire = spwd->sp_expire;
	ent->spwd->sp_flag   = spwd->sp_flag;

	return ent;
}

static struct spdb* _spdb_fgetspent(FILE *input)
{
	assert(input);

	struct spwd *spwd = fgetspent(input);
	if (!spwd) { return NULL; }
	return _spdb_entry(spwd);
}

static void _shadow_free(struct spwd *spwd)
{
	if (!spwd) { return; }
	xfree(spwd->sp_namp);
	xfree(spwd->sp_pwdp);
	xfree(spwd);
}

static void _spdb_entry_free(struct spdb *entry) {
	if (!entry) { return; }
	_shadow_free(entry->spwd);
	free(entry);
}

static struct grdb* _grdb_entry(struct group *group)
{
	assert(group);

	struct grdb *ent;
	ALLOC_ENTRY(struct grdb, ent, struct group, ent->group);

	ent->group->gr_name   = xstrdup(group->gr_name);
	ent->group->gr_gid    = group->gr_gid;
	ent->group->gr_passwd = xstrdup(group->gr_passwd);
	ent->group->gr_mem    = xarrdup(group->gr_mem);

	return ent;
}

static struct grdb* _grdb_fgetgrent(FILE *input)
{
	assert(input);

	struct group *group = fgetgrent(input);
	if (!group) { return NULL; }
	return _grdb_entry(group);
}

static void _group_free(struct group *group)
{
	char **a; /* pointer for gr_mem array traversal */

	if (!group) { return; }
	xfree(group->gr_name);
	xfree(group->gr_passwd);

	/* free the gr_mem array */
	for (a = group->gr_mem; a && *a; a++) { xfree(*a); }
	xfree(group->gr_mem);

	xfree(group);
}

static void _grdb_entry_free(struct grdb *entry)
{
	if (!entry) { return; }
	_group_free(entry->group);
	free(entry);
}

static struct sgdb* _sgdb_entry(struct sgrp *sgrp)
{
	assert(sgrp);

	struct sgdb *ent;
	ALLOC_ENTRY(struct sgdb, ent, struct sgrp, ent->sgrp);

	ent->sgrp->sg_namp   = xstrdup(sgrp->sg_namp);
	ent->sgrp->sg_passwd = xstrdup(sgrp->sg_passwd);
	ent->sgrp->sg_mem    = xarrdup(sgrp->sg_mem);
	ent->sgrp->sg_adm    = xarrdup(sgrp->sg_adm);

	return ent;
}

static struct sgdb* _sgdb_fgetsgent(FILE *input)
{
	assert(input);

	struct sgrp *sgrp = fgetsgent(input);
	if (!sgrp) { return NULL; }
	return _sgdb_entry(sgrp);
}

static void _gshadow_free(struct sgrp *sgrp)
{
	char **a;  /* pointer for sg_mem and sg_adm array traversal */

	if (!sgrp) { return; }
	xfree(sgrp->sg_namp);
	xfree(sgrp->sg_passwd);

	/* free the sg_mem array */
	for (a = sgrp->sg_mem; a && *a; a++) { xfree(*a); }
	xfree(sgrp->sg_mem);

	/* free the sg_adm array */
	for (a = sgrp->sg_adm; a && *a; a++) { xfree(*a); }
	xfree(sgrp->sg_adm);

	xfree(sgrp);
}

static void _sgdb_entry_free(struct sgdb *entry)
{
	if (!entry) { return; }
	_gshadow_free(entry->sgrp);
	free(entry);
}

/**********************************************************/

struct pwdb* pwdb_init(const char *path)
{
	struct pwdb *db, *cur, *entry;
	FILE *input;

	input = fopen(path, "r");
	if (!input) {
		return NULL;
	}

	db = cur = entry = NULL;
	errno = 0;
	while ((entry = _pwdb_fgetpwent(input)) != NULL) {
		if (!db) {
			db = entry;
		} else {
			cur->next = entry;
		}

		cur = entry;
	}

	fclose(input);

	return db;
}

struct passwd* pwdb_get_by_name(struct pwdb *db, const char *name)
{
	assert(name);

	for (; db; db = db->next) {
		if (db->passwd && strcmp(db->passwd->pw_name, name) == 0) {
			return db->passwd;
		}
	}

	return NULL;
}

struct passwd* pwdb_get_by_uid(struct pwdb *db, uid_t uid)
{
	for (; db; db = db->next) {
		if (db->passwd && db->passwd->pw_uid == uid) {
			return db->passwd;
		}
	}

	return NULL;
}

struct passwd* pwdb_new_entry(struct pwdb *db, const char *name)
{
	assert(name);

	struct passwd *pw;

	if (!db) { return NULL; }

	pw = malloc(sizeof(struct passwd));
	if (!pw) { return NULL; }

	/* shallow pointers are ok; _pwdb_entry strdup's them */
	pw->pw_name = (char *)name;
	pw->pw_passwd = "x";
	pw->pw_uid = -1;
	pw->pw_gid = -1;
	pw->pw_gecos = "";
	pw->pw_dir = "/";
	pw->pw_shell = "/sbin/nologin";

	for (; db->next; db = db->next)
		;

	db->next = _pwdb_entry(pw);
	free(pw);

	return (db->next ? db->next->passwd : NULL);
}

int pwdb_add(struct pwdb *db, struct passwd *pw)
{
	struct pwdb *ent;

	if (!db || !pw) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (db->passwd && db->passwd->pw_uid == pw->pw_uid) {
			/* how to signal 'already exists'? */
			return -1;
		}
	}
	ent->next = _pwdb_entry(pw);

	return 0;
}

int pwdb_rm(struct pwdb *db, struct passwd *pw)
{
	struct pwdb *ent = NULL;

	if (!db || !pw) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (db->passwd == pw) {
			if (ent) {
				ent->next = db->next;
				_pwdb_entry_free(db);
				return 0;
			} else {
				/* special case for list head removal */
				_passwd_free(db->passwd);
				db->passwd = NULL;
				return 0;
			}
		}
	}
	return -1;
}

void pwdb_free(struct pwdb *db)
{
	struct pwdb *cur;
	struct pwdb *entry = db;

	if (!db) { return; }

	while (entry) {
		cur = entry->next;
		_pwdb_entry_free(entry);
		entry = cur;
	}
}

int pwdb_write(struct pwdb *db, const char *file)
{
	FILE *output;

	output = fopen(file, "w");
	if (!output) { return -1; }

	for (; db; db = db->next) {
		if (db->passwd && putpwent(db->passwd, output) == -1) {
			fclose(output);
			return -1;
		}
	}
	fclose(output);
	return 0;
}

/**********************************************************/

struct spdb* spdb_init(const char *path)
{
	struct spdb *db, *cur, *entry;
	FILE *input;

	input = fopen(path, "r");
	if (!input) {
		return NULL;
	}

	db = cur = entry = NULL;
	errno = 0;
	while ((entry = _spdb_fgetspent(input)) != NULL) {
		if (!db) {
			db = entry;
		} else {
			cur->next = entry;
		}

		cur = entry;
	}

	fclose(input);

	return db;
}

struct spwd* spdb_get_by_name(struct spdb *db, const char *name)
{
	assert(name);

	struct spdb *ent;
	for (ent = db; ent; ent = ent->next) {
		if (ent->spwd && strcmp(ent->spwd->sp_namp, name) == 0) {
			return ent->spwd;
		}
	}

	return NULL;
}

struct spwd* spdb_new_entry(struct spdb *db, const char *name)
{
	assert(name);

	struct spwd *sp;

	if (!db) { return NULL; }

	sp = calloc(1, sizeof(struct spwd));
	if (!sp) { return NULL; }

	/* shallow pointers are ok; _spdb_entry strdup's them */
	sp->sp_namp = (char *)name;
	sp->sp_pwdp = "!";
	sp->sp_min = 0;
	sp->sp_max = 99999;
	sp->sp_warn = 7;
	sp->sp_inact = 0;
	sp->sp_expire = 0;

	for (; db->next; db = db->next)
		;

	db->next = _spdb_entry(sp);
	free(sp);

	return (db->next ? db->next->spwd : NULL);
}

int spdb_add(struct spdb *db, struct spwd *sp)
{
	struct spdb *ent;

	if (!db || !sp) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (db->spwd && strcmp(db->spwd->sp_namp, sp->sp_namp) == 0) {
			/* how to signal 'already exists'? */
			return -1;
		}
	}
	ent->next = _spdb_entry(sp);

	return 0;
}

int spdb_rm(struct spdb *db, struct spwd *sp)
{
	struct spdb *ent = NULL;

	if (!db || !sp) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (db->spwd == sp) {
			if (ent) {
				ent->next = db->next;
				_spdb_entry_free(db);
				return 0;
			} else {
				/* special case for list head removal */
				_shadow_free(db->spwd);
				db->spwd = NULL;
				return 0;
			}
		}
	}
	return -1;
}

void spdb_free(struct spdb *db)
{
	struct spdb *cur;
	struct spdb *entry = db;

	if (!db) { return; }

	while (entry) {
		cur = entry->next;
		_spdb_entry_free(entry);
		entry = cur;
	}
}

int spdb_write(struct spdb *db, const char *file)
{
	FILE *output;

	output = fopen(file, "w");
	if (!output) { return -1; }

	for (; db; db = db->next) {
		if (db->spwd && putspent(db->spwd, output) == -1) {
			fclose(output);
			return -1;
		}
	}
	fclose(output);
	return 0;
}

/**********************************************************/

struct grdb* grdb_init(const char *path)
{
	struct grdb *db, *cur, *entry;
	FILE *input;

	input = fopen(path, "r");
	if (!input) {
		return NULL;
	}

	db = cur = entry = NULL;
	errno = 0;
	while ((entry = _grdb_fgetgrent(input)) != NULL) {
		if (!db) {
			db = entry;
		} else {
			cur->next = entry;
		}

		cur = entry;
	}

	fclose(input);

	return db;
}

struct group* grdb_get_by_name(struct grdb *db, const char *name)
{
	assert(name);

	for (; db; db = db->next) {
		if (db->group && strcmp(db->group->gr_name, name) == 0) {
			return db->group;
		}
	}

	return NULL;
}

struct group* grdb_get_by_gid(struct grdb *db, gid_t gid)
{
	for (; db; db = db->next) {
		if (db->group && db->group->gr_gid == gid) {
			return db->group;
		}
	}

	return NULL;
}

struct group* grdb_new_entry(struct grdb *db, const char *name)
{
	assert(name);

	struct group *gr;

	if (!db) { return NULL; }

	gr = malloc(sizeof(struct group));
	if (!gr) { return NULL; }

	/* shallow pointers are ok; _grdb_entry strdup's them */
	gr->gr_name = (char *)name;
	gr->gr_passwd = "x";
	gr->gr_gid = -1;
	gr->gr_mem = NULL;

	for (; db->next; db = db->next)
		;

	db->next = _grdb_entry(gr);
	free(gr);

	return (db->next ? db->next->group : NULL);
}

int grdb_add(struct grdb *db, struct group *g)
{
	struct grdb *ent;

	if (!db || !g) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (db->group && db->group->gr_gid == g->gr_gid) {
			/* how to signal 'already exists'? */
			return -1;
		}
	}
	ent->next = _grdb_entry(g);

	return 0;
}

int grdb_rm(struct grdb *db, struct group *g)
{
	struct grdb *ent = NULL;

	if (!db || !g) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (db->group == g) {
			if (ent) {
				ent->next = db->next;
				_grdb_entry_free(db);
				return 0;
			} else {
				/* special case for list head removal */
				_group_free(db->group);
				db->group = NULL;
				return 0;
			}
		}
	}
	return -1;
}

int grdb_write(struct grdb *db, const char *file)
{
	FILE *output;

	output = fopen(file, "w");
	if (!output) { return -1; }

	for (; db; db = db->next) {
		if (db->group && putgrent(db->group, output) == -1) {
			fclose(output);
			return -1;
		}
	}
	fclose(output);
	return 0;
}

void grdb_free(struct grdb *db)
{
	struct grdb *cur;
	struct grdb *ent = db;

	if (!db) { return; }

	while (ent) {
		cur = ent->next;
		_grdb_entry_free(ent);
		ent = cur;
	}
}

/**********************************************************/

struct sgdb* sgdb_init(const char *path)
{
	struct sgdb *db, *cur, *entry;
	FILE *input;

	input = fopen(path, "r");
	if (!input) {
		return NULL;
	}

	db = cur = entry = NULL;
	errno = 0;
	while ((entry = _sgdb_fgetsgent(input)) != NULL) {
		if (!db) {
			db = entry;
		} else {
			cur->next = entry;
		}

		cur = entry;
	}

	fclose(input);

	return db;
}

struct sgrp* sgdb_get_by_name(struct sgdb *db, const char *name)
{
	assert(name);

	for (; db; db = db->next) {
		if (db->sgrp && strcmp(db->sgrp->sg_namp, name) == 0) {
			return db->sgrp;
		}
	}

	return NULL;
}

struct sgrp* sgdb_new_entry(struct sgdb *db, const char *name)
{
	assert(name);

	struct sgrp *sg;

	if (!db) { return NULL; }

	sg = calloc(1, sizeof(struct sgrp));
	if (!sg) { return NULL; }

	/* shallow pointers are ok; _sgdb_entry strdup's them */
	sg->sg_namp = xstrdup(name);
	sg->sg_passwd = "!";

	for (; db->next; db = db->next)
		;

	db->next = _sgdb_entry(sg);
	xfree(sg->sg_namp);
	free(sg);

	return (db->next ? db->next->sgrp : NULL);
}

int sgdb_add(struct sgdb *db, struct sgrp *g)
{
	struct sgdb *ent;

	if (!db || !g) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (db->sgrp && strcmp(db->sgrp->sg_namp, g->sg_namp) == 0) {
			/* how to signal 'already exists'? */
			return -1;
		}
	}
	ent->next = _sgdb_entry(g);

	return 0;
}

int sgdb_rm(struct sgdb *db, struct sgrp *g)
{
	struct sgdb *ent = NULL;

	if (!db || !g) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (db->sgrp == g) {
			if (ent) {
				ent->next = db->next;
				_sgdb_entry_free(db);
				return 0;
			} else {
				/* special case for list head removal */
				_gshadow_free(db->sgrp);
				db->sgrp = NULL;
				return 0;
			}
		}
	}
	return -1;
}

int sgdb_write(struct sgdb *db, const char *file)
{
	FILE *output;

	output = fopen(file, "w");
	if (!output) { return -1; }

	for (; db; db = db->next) {
		if (db->sgrp && putsgent(db->sgrp, output) == -1) {
			fclose(output);
			return -1;
		}
	}
	fclose(output);
	return 0;
}

void sgdb_free(struct sgdb *db)
{
	struct sgdb *ent;

	while (db) {
		ent = db->next;
		_sgdb_entry_free(db);
		db = ent;
	}
}

