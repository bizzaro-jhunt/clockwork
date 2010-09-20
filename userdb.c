#define _SVID_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "userdb.h"

static struct pwdb* _pwdb_entry(struct passwd *passwd);
static struct pwdb* _pwdb_fgetpwent(FILE *input);
static void _pwdb_entry_free(struct pwdb *entry);

static struct spdb* _spdb_entry(struct spwd *spwd);
static struct spdb* _spdb_fgetpwent(FILE *input);
static void _spdb_entry_free(struct spdb *entry);

/**********************************************************/

static struct pwdb* _pwdb_entry(struct passwd *passwd)
{
	assert(passwd);

	struct pwdb *ent = malloc(sizeof(struct pwdb));
	if (!ent) { return NULL; }

	ent->next = NULL;

	ent->passwd.pw_name   = strdup(passwd->pw_name);
	ent->passwd.pw_passwd = strdup(passwd->pw_passwd);
	ent->passwd.pw_uid    = passwd->pw_uid;
	ent->passwd.pw_gid    = passwd->pw_gid;
	ent->passwd.pw_gecos  = strdup(passwd->pw_gecos);
	ent->passwd.pw_dir    = strdup(passwd->pw_dir);
	ent->passwd.pw_shell  = strdup(passwd->pw_shell);

	return ent;
}

static struct pwdb* _pwdb_fgetpwent(FILE *input)
{
	assert(input);

	struct passwd *passwd = fgetpwent(input);
	if (!passwd) { return NULL; }
	return _pwdb_entry(passwd);
}

static void _pwdb_entry_free(struct pwdb *entry) {
	free(entry->passwd.pw_name);
	free(entry->passwd.pw_passwd);
	free(entry->passwd.pw_gecos);
	free(entry->passwd.pw_dir);
	free(entry->passwd.pw_shell);
	free(entry);
}

static struct spdb* _spdb_entry(struct spwd *spwd)
{
	assert(spwd);

	struct spdb *ent = malloc(sizeof(struct spdb));
	if (!ent) { return NULL; }

	ent->next = NULL;

	ent->spwd.sp_namp   = strdup(spwd->sp_namp);
	ent->spwd.sp_pwdp   = strdup(spwd->sp_pwdp);
	ent->spwd.sp_lstchg = spwd->sp_lstchg;
	ent->spwd.sp_min    = spwd->sp_min;
	ent->spwd.sp_max    = spwd->sp_max;
	ent->spwd.sp_warn   = spwd->sp_warn;
	ent->spwd.sp_inact  = spwd->sp_inact;
	ent->spwd.sp_expire = spwd->sp_expire;
	ent->spwd.sp_flag   = spwd->sp_flag;

	return ent;
}

static struct spdb* _spdb_fgetspent(FILE *input)
{
	assert(input);

	struct spwd *spwd = fgetspent(input);
	if (!spwd) { return NULL; }
	return _spdb_entry(spwd);
}

static void _spdb_entry_free(struct spdb *entry) {
	free(entry->spwd.sp_namp);
	free(entry->spwd.sp_pwdp);
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
	while (entry = _pwdb_fgetpwent(input)) {
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
		if (strcmp(db->passwd.pw_name, name) == 0) {
			return &(db->passwd);
		}
	}

	return NULL;
}

struct passwd* pwdb_get_by_uid(struct pwdb *db, uid_t uid)
{
	for (; db; db = db->next) {
		if (db->passwd.pw_uid == uid) {
			return &(db->passwd);
		}
	}

	return NULL;
}

int pwdb_add(struct pwdb *db, struct passwd *pw)
{
	struct pwdb *ent;

	if (!db || !pw) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (db->passwd.pw_uid == pw->pw_uid) {
			/* how to signal 'already exists'? */
			return -1;
		}
	}
	ent->next = _pwdb_entry(pw);

	return 0;
}

int pwdb_rm(struct pwdb *db, struct passwd *pw)
{
	struct pwdb *ent;

	if (!db || !pw) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (&(db->passwd) == pw) {
			if (ent) { /* FIXME: handle removal of list head */
				ent->next = db->next;
				_pwdb_entry_free(db);
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
		if (putpwent(&(db->passwd), output) == -1) {
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
	while (entry = _spdb_fgetspent(input)) {
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
		if (strcmp(ent->spwd.sp_namp, name) == 0) {
			return &(ent->spwd);
		}
	}

	return NULL;
}

int spdb_add(struct spdb *db, struct spwd *sp)
{
	struct spdb *ent;

	if (!db || !sp) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (strcmp(db->spwd.sp_namp, sp->sp_namp) == 0) {
			/* how to signal 'already exists'? */
			return -1;
		}
	}
	ent->next = _spdb_entry(sp);

	return 0;
}

int spdb_rm(struct spdb *db, struct spwd *sp)
{
	struct spdb *ent;

	if (!db || !sp) { return -1; }

	for (; db; ent = db, db = db->next) {
		if (&(db->spwd) == sp) {
			if (ent) { /* FIXME: handle removal of list head */
				ent->next = db->next;
				_spdb_entry_free(db);
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

		free(entry->spwd.sp_namp);
		free(entry->spwd.sp_pwdp);
		free(entry);

		entry = cur;
	}
}

int spdb_write(struct spdb *db, const char *file)
{
	FILE *output;

	output = fopen(file, "w");
	if (!output) { return -1; }

	for (; db; db = db->next) {
		if (putspent(&(db->spwd), output) == -1) {
			fclose(output);
			return -1;
		}
	}
	fclose(output);
	return 0;
}

