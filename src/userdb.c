/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#define _SVID_SOURCE
#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "userdb.h"

/**********************************************************/

/* for older versions of glibc */
static char SGBUF[8192], *SGADM[128], *SGMEM[128];
static struct sgrp SGENT;
static struct sgrp* fgetsgent(FILE *io)
{
	char *a, *b, *c;
	if (feof(io) || !fgets(SGBUF, 8191, io)) { return NULL; }

	SGENT.sg_mem = SGMEM;
	SGENT.sg_adm = SGADM;

	/* name */
	for (a = SGBUF, b = a; *b && *b != ':'; b++)
		;
	*b = '\0';
	SGENT.sg_namp = a;

	/* password */
	for (a = ++b; *b && *b != ':'; b++)
		;
	*b = '\0';
	SGENT.sg_passwd = a;

	/* admins */
	for (a = ++b; *b && *b != ':'; b++)
		;
	*b = '\0'; c = a;
	while (c < b) {
		for (a = c; *c && *c != ','; c++)
			;
		*c++ = '\0';
		*(SGENT.sg_adm++) = a;
	};
	*(SGENT.sg_adm) = NULL;

	/* members */
	for (a = ++b; *b && *b != '\n'; b++)
		;
	*b = '\0'; c = a;
	while (c < b) {
		for (a = c; *c && *c != ','; c++)
			;
		*c++ = '\0';
		*(SGENT.sg_mem++) = a;
	};
	*(SGENT.sg_mem) = NULL;

	SGENT.sg_mem = SGMEM;
	SGENT.sg_adm = SGADM;

	return &SGENT;
}

static int putsgent(const struct sgrp *g, FILE *io)
{
	char *members, *admins;
	struct stringlist *mem, *adm;
	int ret;

	mem = stringlist_new(g->sg_mem);
	members = stringlist_join(mem, ",");
	stringlist_free(mem);

	adm  = stringlist_new(g->sg_adm);
	admins = stringlist_join(adm, ",");
	stringlist_free(adm);

	ret = fprintf(io, "%s:%s:%s:%s\n", g->sg_namp, g->sg_passwd, admins, members);
	free(admins); free(members);

	return ret;
}

/**********************************************************/

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
	assert(passwd); // LCOV_EXCL_LINE

	struct pwdb *ent;
	ent = xmalloc(sizeof(struct pwdb));
	ent->passwd = xmalloc(sizeof(struct passwd));

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
	assert(input); // LCOV_EXCL_LINE

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
	assert(spwd); // LCOV_EXCL_LINE

	struct spdb *ent;
	ent = xmalloc(sizeof(struct spdb));
	ent->spwd = xmalloc(sizeof(struct spwd));

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
	assert(input); // LCOV_EXCL_LINE

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
	assert(group); // LCOV_EXCL_LINE

	struct grdb *ent;
	ent = xmalloc(sizeof(struct grdb));
	ent->group = xmalloc(sizeof(struct group));

	ent->group->gr_name   = xstrdup(group->gr_name);
	ent->group->gr_gid    = group->gr_gid;
	ent->group->gr_passwd = xstrdup(group->gr_passwd);
	ent->group->gr_mem    = xarrdup(group->gr_mem);

	return ent;
}

static struct grdb* _grdb_fgetgrent(FILE *input)
{
	assert(input); // LCOV_EXCL_LINE

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
	assert(sgrp); // LCOV_EXCL_LINE

	struct sgdb *ent;
	ent = xmalloc(sizeof(struct sgdb));
	ent->sgrp = xmalloc(sizeof(struct sgrp));

	ent->sgrp->sg_namp   = xstrdup(sgrp->sg_namp);
	ent->sgrp->sg_passwd = xstrdup(sgrp->sg_passwd);
	ent->sgrp->sg_mem    = xarrdup(sgrp->sg_mem);
	ent->sgrp->sg_adm    = xarrdup(sgrp->sg_adm);

	return ent;
}

static struct sgdb* _sgdb_fgetsgent(FILE *input)
{
	assert(input); // LCOV_EXCL_LINE

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

/**
  Open the user database at $path

  The pointer returned by this function must be passed to
  @pwdb_free in order to properly release all memory allocated
  dynamically on the heap.

  On success, returns a pointer to a password database.
  On failure, returns false.
 */
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

/**
  Find a user in $db by $name

  If the user is found, returns a pointer to its `passwd` structure;
  see `getpwent(3)`.  Otherwise, returns NULL.
 */
struct passwd* pwdb_get_by_name(struct pwdb *db, const char *name)
{
	assert(name); // LCOV_EXCL_LINE

	for (; db; db = db->next) {
		if (db->passwd && strcmp(db->passwd->pw_name, name) == 0) {
			return db->passwd;
		}
	}

	return NULL;
}

/**
  Find UID in $db by $name

  If found, returns the UID of the user account via the
  $uid value-result argument and returns zero.

  Otherwise, returns a non-zero value.
 */
int pwdb_lookup_uid(struct pwdb *db, const char *name, uid_t *uid)
{
	if (!name || !uid) return -1;

	struct passwd* u = pwdb_get_by_name(db, name);
	if (!u) return -1;

	*uid = u->pw_uid;
	return 0;
}

/**
  Find next unused UID in $db

  Useful for creating "auto-increment" UIDs, pwdb_next_uid
  starts at UID 1000 and tries to find the next available
  user ID, based on what IDs have already been handed out.

  It emulates the automatic UID generation of the standard
  useradd command (i.e. highest in use + 1)
 */
uid_t pwdb_next_uid(struct pwdb *db)
{
	uid_t next = 1000;

	for (; db; db = db->next) {
		if (db->passwd && db->passwd->pw_uid > next) {
			next = db->passwd->pw_uid+1;
		}
	}

	return next;
}

/**
  Find a user in $db by $uid

  If the user is found, returns a pointer to its `passwd` structure;
  see `getpwent(3)`.  Otherwise, returns NULL.
 */
struct passwd* pwdb_get_by_uid(struct pwdb *db, uid_t uid)
{
	for (; db; db = db->next) {
		if (db->passwd && db->passwd->pw_uid == uid) {
			return db->passwd;
		}
	}

	return NULL;
}

/**
  Create a new user in $db

  The username ($name), UID, ($uid) and GID ($gid)
  are specified by the caller.

  See @spdb_new_entry.

  On success, returns a pointer to the new `passwd` structure.
  On failure, returns NULL.
 */
struct passwd* pwdb_new_entry(struct pwdb *db, const char *name, uid_t uid, gid_t gid)
{
	assert(name); // LCOV_EXCL_LINE

	struct passwd *pw;

	if (!db) { return NULL; }

	pw = xmalloc(sizeof(struct passwd));
	/* shallow pointers are ok; _pwdb_entry strdup's them */
	pw->pw_name = (char *)name;
	pw->pw_passwd = "x";
	pw->pw_uid = (uid == -1 ? pwdb_next_uid(db) : uid);
	pw->pw_gid = gid;
	pw->pw_gecos = "";
	pw->pw_dir = "/";
	pw->pw_shell = "/sbin/nologin";

	for (; db->next; db = db->next)
		;

	db->next = _pwdb_entry(pw);
	free(pw);

	return (db->next ? db->next->passwd : NULL);
}

/**
  Add a user account to $db

  If an entry already exists in $db with the same UID as $pw,
  the operation fails.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Remove a user from $db.

  This function evaluates each entry by comparing the passwd
  pointer to the $pw argument.  For that reason, callers must
  use a pointer returned by @pwdb_get_by_name or @pwdb_get_by_uid;
  it is not sufficient to populate a new `passwd` structure and
  pass it to this function.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Free $db.
 */
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

/**
  Write $db to $file.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Open the shadow database at $path.

  The pointer returned by this function must be passed to
  @spdb_free in order to properly release all memory allocated
  dynamically on the heap.

  On success, returns a pointer to a shadow database.
  On failure, returns NULL.
 */
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

/**
  Find a user in $db by $name

  If the user is found, returns a pointer to its `shadow`
  structure; see `getspent(3)`.  Otherwise, returns NULL.
 */
struct spwd* spdb_get_by_name(struct spdb *db, const char *name)
{
	assert(name); // LCOV_EXCL_LINE

	struct spdb *ent;
	for (ent = db; ent; ent = ent->next) {
		if (ent->spwd && strcmp(ent->spwd->sp_namp, name) == 0) {
			return ent->spwd;
		}
	}

	return NULL;
}

/**
  Create a new user ($name) in $db.

  See @pwdb_new_entry.

  On success, returns a pointer to the new passwd structure.
  On failure, returns NULL.
 */
struct spwd* spdb_new_entry(struct spdb *db, const char *name)
{
	assert(name); // LCOV_EXCL_LINE

	struct spwd *sp;

	if (!db) { return NULL; }

	sp = xmalloc(sizeof(struct spwd));
	/* shallow pointers are ok; _spdb_entry strdup's them */
	sp->sp_namp = (char *)name;
	sp->sp_pwdp = "!";
	sp->sp_min = -1;
	sp->sp_max = 99999;
	sp->sp_warn = 7;
	sp->sp_inact = -1;
	sp->sp_expire = -1;
	sp->sp_flag = 0;

	for (; db->next; db = db->next)
		;

	db->next = _spdb_entry(sp);
	free(sp);

	return (db->next ? db->next->spwd : NULL);
}

/**
  Add a user account to $db.

  If an entry already exists in $db with the same name as
  $sp, the operation fails.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Remove a user from $db.

  This function evaluates each entry by comparing the spwd
  pointer to $sp.  For that reason, callers must use a pointer
  returned by @spdb_get_by_name; it is not sufficient to populate
  a new `spwd` structure and pass it to this function.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Free $db.
 */
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

/**
  Write $db to $file.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Open the group database at $path.

  The pointer returned by this function must be passed to
  @grdb_free in order to properly release all memory allocated
  dynamically on the heap.

  On success, returns a pointer to a group database.
  On failure, returns NULL.
 */
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

/**
  Find a group named $nae, in $db.

  If the group is found, returns a pointer to its `group`
  structure; see `getgrent(3)`.  Otherwise, returns NULL.
 */
struct group* grdb_get_by_name(struct grdb *db, const char *name)
{
	assert(name); // LCOV_EXCL_LINE

	for (; db; db = db->next) {
		if (db->group && strcmp(db->group->gr_name, name) == 0) {
			return db->group;
		}
	}

	return NULL;
}

/**
  Find GID in $db by $name

  If found, returns the GID of the group account via the
  $gid value-result argument and returns zero.

  Otherwise, returns a non-zero value.
 */
int grdb_lookup_gid(struct grdb *db, const char *name, gid_t *gid)
{
	if (!name || !gid) return -1;

	struct group *g = grdb_get_by_name(db, name);
	if (!g) return -1;

	*gid = g->gr_gid;
	return 0;
}

/**
  Find a group in $db with GID of $gid.

  If found, a pointer to the `group` structure is returned;
  see `getgrent(3)`.
  Otherwise, returns NULL.
 */
struct group* grdb_get_by_gid(struct grdb *db, gid_t gid)
{
	for (; db; db = db->next) {
		if (db->group && db->group->gr_gid == gid) {
			return db->group;
		}
	}

	return NULL;
}

/**
  Create a new group in $db.

  The group name ($name) and GID ($gid) are specified
  by the caller.

  See @sgdb_new_entry.

  On success, returns a pointer to the new `group` structure.
  On failure, returns NULL.
 */
struct group* grdb_new_entry(struct grdb *db, const char *name, gid_t gid)
{
	assert(name); // LCOV_EXCL_LINE

	struct group *gr;

	if (!db) { return NULL; }

	gr = xmalloc(sizeof(struct group));
	/* shallow pointers are ok; _grdb_entry strdup's them */
	gr->gr_name = (char *)name;
	gr->gr_passwd = "x";
	gr->gr_gid = gid;
	gr->gr_mem = NULL;

	for (; db->next; db = db->next)
		;

	db->next = _grdb_entry(gr);
	free(gr);

	return (db->next ? db->next->group : NULL);
}

/**
  Add a group account to $db.

  If an entry already exists in $db with the same GID as $g,
  the operation fails.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Remove a group from $db.

  This function evaluates each entry by comparing the group
  pointer to $g.  For that reason, callers must use a pointer
  returned by @grdb_get_by_name or @grdb_get_by_gid;
  it is not sufficient to populate a new group structure and
  pass it to this function.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Write $db to $file.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Free $db.
 */
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

/**
  Open the group shadow database at $path

  The pointer returned by this function must be passed to
  @sgdb_free in order to properly release all memory allocated
  dynamically on the heap.

  On success, returns a pointer to a group shadow database.
  On failure, returns false.
 */
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

/**
  Find a group in $db, by $name.

  If the group is found, returns a pointer to its `sgrp` structure.
  Otherwise, returns NULL.
 */
struct sgrp* sgdb_get_by_name(struct sgdb *db, const char *name)
{
	assert(name); // LCOV_EXCL_LINE

	for (; db; db = db->next) {
		if (db->sgrp && strcmp(db->sgrp->sg_namp, name) == 0) {
			return db->sgrp;
		}
	}

	return NULL;
}

/**
  Create a new shadow group in $db.

  On success, returns a pointer to the new `sgrp` structure.
  On failure, returns NULL.
 */
struct sgrp* sgdb_new_entry(struct sgdb *db, const char *name)
{
	assert(name); // LCOV_EXCL_LINE

	struct sgrp *sg;

	if (!db) { return NULL; }

	sg = xmalloc(sizeof(struct sgrp));
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

/**
  Add a group account to $db

  If an entry already exists in $db with the same name as $g,
  the operation fails.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Remove a group from $db.

  This function evaluates each entry by comparing the passwd
  pointer to $g.  For that reason, callers must use a pointer
  returned by @sgdb_get_by_name;
  it is not sufficient to populate a new `sgrp` structure and
  pass it to this function.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Write $db to $file.

  On success, returns 0.  On failure, returns non-zero.
 */
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

/**
  Free $db.
 */
void sgdb_free(struct sgdb *db)
{
	struct sgdb *ent;

	while (db) {
		ent = db->next;
		_sgdb_entry_free(db);
		db = ent;
	}
}

