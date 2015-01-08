/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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

#include "authdb.h"

#define LINEMAX 8192

authdb_t* authdb_read(const char *root, int dbs)
{
	user_t *user;
	group_t *group;
	FILE *io;
	char *file, *a, *b, LINE[LINEMAX];

	authdb_t *db = vmalloc(sizeof(authdb_t));
	list_init(&db->users);
	list_init(&db->groups);
	db->dbs = dbs;
	db->root = strdup(root);

	if (db->dbs & AUTHDB_PASSWD) {
		file = string("%s/passwd",  db->root);
		io = fopen(file, "r"); free(file);
		if (!io) goto bail;

		while ((fgets(LINE, LINEMAX - 1, io)) != NULL) {
			a = b = LINE;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user = user_find(db, a, -1);
			if (!user) {
				user = vmalloc(sizeof(user_t));
				list_push(&db->users, &user->l);
				user->name = strdup(a);
			}
			user->state |= AUTHDB_PASSWD;

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->clear_pass = strdup(a);

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->uid = atoi(a); /* FIXME: use strtol + errno */

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->gid = atoi(a); /* FIXME: use strtol + errno */

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->comment = strdup(a);

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->home = strdup(a);

			a = ++b; /*                       LAST field */
			while (*b && *b != ':') b++; if (*b) goto bail;
			user->shell = strdup(a);
		}

		fclose(io);
	}

	if (db->dbs & AUTHDB_SHADOW) {
		file = string("%s/shadow",  db->root);
		io = fopen(file, "r"); free(file);
		if (!io) goto bail;

		while ((fgets(LINE, LINEMAX - 1, io)) != NULL) {
			a = b = LINE;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user = user_find(db, a, -1);
			if (!user) {
				user = user_add(db);
				user->name = strdup(a);
			}
			user->state |= AUTHDB_SHADOW;

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->crypt_pass = strdup(a);

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->creds.last_changed = atoi(a); /* FIXME: use strtol + errno */

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->creds.min_days = atoi(a); /* FIXME: use strtol + errno */

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->creds.max_days = atoi(a); /* FIXME: use strtol + errno */

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->creds.grace_period = atoi(a); /* FIXME: use strtol + errno */

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			user->creds.expiration = atoi(a); /* FIXME: use strtol + errno */

			a = ++b; /*                       LAST field */
			while (*b && *b != ':') b++; if (*b) goto bail;
			user->creds.flags = atoi(a); /* FIXME: use strtoul + errno */
		}

		fclose(io);
	}

	if (db->dbs & AUTHDB_GROUP) {
		file = string("%s/group",  db->root);
		io = fopen(file, "r"); free(file);
		if (!io) goto bail;

		while ((fgets(LINE, LINEMAX - 1, io)) != NULL) {
			a = b = LINE;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			group = group_find(db, a, -1);
			if (!group) {
				group = group_add(db);
				group->name = strdup(a);
			}
			group->state |= AUTHDB_GROUP;

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			group->clear_pass = strdup(a);

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			group->gid = atoi(a); /* FIXME: use strtol + errno */

			a = ++b; /*                       LAST field */
			while (*b && *b != ':') b++; if (*b) goto bail;
			group->raw_members = strdup(a);
		}

		fclose(io);
	}

	if (db->dbs & AUTHDB_GSHADOW) {
		file = string("%s/gshadow",  db->root);
		io = fopen(file, "r"); free(file);
		if (!io) goto bail;

		while ((fgets(LINE, LINEMAX - 1, io)) != NULL) {
			a = b = LINE;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			group = group_find(db, a, -1);
			if (!group) {
				group = group_add(db);
				group->name = strdup(a);
			}
			group->state |= AUTHDB_GSHADOW;

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			group->crypt_pass = strdup(a);

			a = ++b;
			while (*b && *b != ':') b++; if (!*b) goto bail; *b = '\0';
			group->raw_admins = strdup(a);

			a = ++b; /*                       LAST field */
			while (*b && *b != ':') b++; if (*b) goto bail;
			free(group->raw_members);
			group->raw_members = strdup(a);
		}

		fclose(io);
	}

	return db;

bail:
	fclose(io);
	authdb_close(db);
	return NULL;
}

int authdb_write(authdb_t *db)
{
	user_t *user;
	group_t *group;
	FILE *io;
	char *tmpfile, *file;
	int rc;

	if (db->dbs & AUTHDB_PASSWD) {
		file = string("%s/passwd", db->root);
		tmpfile = string("%s/.passwd.%x", db->root, rand());

		io = fopen(tmpfile, "w");
		if (!io) goto bail;

		for_each_object(user, &db->users, l) {
			fprintf(io, "%s:%s:%u:%u:%s:%s:%s\n",
				user->name, user->clear_pass, user->uid, user->gid,
				user->comment, user->home, user->shell);
		}

		fclose(io);
		rc = rename(tmpfile, file);
		if (rc != 0) {
			unlink(tmpfile);
			goto bail;
		}
	}

	if (db->dbs & AUTHDB_SHADOW) {
		file = string("%s/shadow", db->root);
		tmpfile = string("%s/.shadow.%x", db->root, rand());
		io = fopen(tmpfile, "w");
		if (!io) goto bail;

		for_each_object(user, &db->users, l) {
#define null_string(f,s) !user->creds.f ? string("") : string(s, user->creds.f)
			char *pwmin  = null_string(min_days,     "%li");
			char *pwmax  = null_string(max_days,     "%li");
			char *pwwarn = null_string(warn_days,    "%li");
			char *inact  = null_string(grace_period, "%li");
			char *expiry = null_string(expiration,   "%li");
			char *flags  = null_string(flags,        "%lu");
#undef null_string

			fprintf(io, "%s:%s:%lu:%s:%s:%s:%s:%s:%s\n",
				user->name, user->crypt_pass, user->creds.last_changed,
				pwmin, pwmax, pwwarn, inact, expiry, flags);

			free(pwmin); free(pwmax);  free(pwwarn);
			free(inact); free(expiry); free(flags);
		}

		fclose(io);
		rc = rename(tmpfile, file);
		if (rc != 0) {
			unlink(tmpfile);
			goto bail;
		}
	}

	if (db->dbs & AUTHDB_GROUP) {
		file = string("%s/group", db->root);
		tmpfile = string("%s/.group.%x", db->root, rand());

		io = fopen(tmpfile, "w");
		if (!io) goto bail;

		for_each_object(group, &db->groups, l) {
			fprintf(io, "%s:%s:%u:%s\n",
				group->name, group->clear_pass, group->gid, group->raw_members);
		}

		fclose(io);
		rc = rename(tmpfile, file);
		if (rc != 0) {
			unlink(tmpfile);
			goto bail;
		}
	}

	if (db->dbs & AUTHDB_GSHADOW) {
		file = string("%s/gshadow", db->root);
		tmpfile = string("%s/.gshadow.%x", db->root, rand());

		io = fopen(tmpfile, "w");
		if (!io) goto bail;

		for_each_object(group, &db->groups, l) {
			fprintf(io, "%s:%s:%s:%s\n",
				group->name, group->crypt_pass,
				group->raw_admins, group->raw_members);
		}

		fclose(io);
		rc = rename(tmpfile, file);
		if (rc != 0) {
			unlink(tmpfile);
			goto bail;
		}
	}

	return 0;
bail:
	fclose(io);
	return 1;
}

void authdb_close(authdb_t *db)
{
}


char* authdb_creds(authdb_t *db, const char *username)
{
	user_t *user = user_find(db, username, -1);
	if (!user) return NULL;

	group_t *group = group_find(db, NULL, user->gid);
	if (!group) return NULL;

	strings_t *creds = strings_new(NULL);
	strings_add(creds, user->name);
	strings_add(creds, group->name);

	member_t *member;
	for_each_object(member, &user->member_of, l)
		strings_add(creds, member->name);

	char *list = strings_join(creds, ":");
	strings_free(creds);
	return list;
}


uid_t authdb_nextuid(authdb_t *db, uid_t uid)
{
	user_t *user;
	NEXT:
	for_each_object(user, &db->users, l) {
		if (user->uid != uid) continue;
		uid++; goto NEXT;
	}
	return uid;
}

gid_t authdb_nextgid(authdb_t *db, gid_t gid)
{
	group_t *group;
	NEXT:
	for_each_object(group, &db->groups, l) {
		if (group->gid != gid) continue;
		gid++; goto NEXT;
	}
	return gid;
}


user_t* user_find(authdb_t *db, const char *name, uid_t uid)
{
	user_t *user;

	if (name) {
		for_each_object(user, &db->users, l)
			if (strcmp(user->name, name) == 0)
				return user;
	} else {
		for_each_object(user, &db->users, l)
			if (user->uid == uid)
				return user;
	}
	return NULL;
}

user_t* user_add(authdb_t *db)
{
	user_t *user = vmalloc(sizeof(user_t));
	list_push(&db->users, &user->l);
	return user;
}

void user_remove(user_t *user)
{
	if (!user) return;
	list_delete(&user->l);
	free(user->name);
	free(user->clear_pass);
	free(user->crypt_pass);
	free(user->comment);
	free(user->home);
	free(user->shell);
	free(user);
}


group_t* group_find(authdb_t *db, const char *name, gid_t gid)
{
	group_t *group;

	if (name) {
		for_each_object(group, &db->groups, l)
			if (strcmp(group->name, name) == 0)
				return group;
	} else {
		for_each_object(group, &db->groups, l)
			if (group->gid == gid)
				return group;
	}
	return NULL;
}

group_t* group_add(authdb_t *db)
{
	group_t *group = vmalloc(sizeof(group_t));
	list_push(&db->groups, &group->l);
	return group;
}

void group_remove(group_t *group)
{
	if (!group) return;
	list_delete(&group->l);
	free(group->name);
	free(group->clear_pass);
	free(group->crypt_pass);
	free(group->raw_members);
	free(group->raw_admins);
	free(group);
}
