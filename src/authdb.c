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

#define _DEFAULT_SOURCE
#define _SVID_SOURCE
#define _GNU_SOURCE

#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>

#include "authdb.h"

#define LINEMAX 8192

static void s_add_member(authdb_t *db, int type, group_t *group, user_t *user)
{
	if (!group || !user) return;
	assert(type == GROUP_MEMBER || type == GROUP_ADMIN);

	list_t *group_list = (type == GROUP_MEMBER ? &group->members  : &group->admins);
	list_t *user_list  = (type == GROUP_MEMBER ? &user->member_of : &user->admin_of);

	member_t *member = vmalloc(sizeof(member_t));
	member->user  = user;  list_init(&member->on_user);
	member->group = group; list_init(&member->on_group);
	                       list_init(&member->l);

	member_t *needle;
	for_each_object(needle, group_list, on_group)
		if (needle->group == group && needle->user == user)
			goto check_user;

	list_push(group_list, &member->on_group);
	member->refs++;

check_user:
	for_each_object(needle, user_list, on_user)
		if (needle->group == group && needle->user == user)
			goto done;

	list_push(user_list, &member->on_user);
	member->refs++;

done:
	if (member->refs == 0)
		free(member);
	else
		list_push(&db->memberships, &member->l);
}

static void s_member_free(member_t *m)
{
	if (!m) return;
	list_delete(&m->l);
	list_delete(&m->on_user);
	list_delete(&m->on_group);
	free(m);
}

static void s_remove_member(authdb_t *db, int type, group_t *group, user_t *user)
{
	if (!group || !user) return;
	assert(type == GROUP_MEMBER || type == GROUP_ADMIN);

	list_t *group_list = (type == GROUP_MEMBER ? &group->members : &group->admins);

	member_t *needle;
	for_each_object(needle, group_list, on_group) {
		if (needle->group != group || needle->user != user)
			continue;
		s_member_free(needle);
		return;
	}
}

static char* s_group_list(group_t *group, int type)
{
	strings_t *lst = strings_new(NULL);
	list_t *group_list = (type == GROUP_MEMBER ? &group->members : &group->admins);

	member_t *member;
	for_each_object(member, group_list, on_group)
		strings_add(lst, member->user->name);

	char *s = strings_join(lst, ",");
	strings_free(lst);

	return s;
}

authdb_t* authdb_read(const char *root, int dbs)
{
	user_t *user;
	group_t *group;
	FILE *io;
	size_t line, field;
	char *file, *a, *b, LINE[LINEMAX];
	char *endptr; /* for strtoul calls */

	authdb_t *db = vmalloc(sizeof(authdb_t));
	list_init(&db->users);
	list_init(&db->groups);
	list_init(&db->memberships);
	db->dbs = dbs;
	db->root = strdup(root);

#define FIELD(d,n) do { \
	field++; \
	while (*b && *b != ':') b++; \
	if (!*b) { \
		fprintf(stderr, "authdb_read encountered an error on line %li of the "d" database: " \
	                    "line ended prematurely while reading the '"n"' field (#%li)\n", \
	                    (long)line, (long)field); \
		goto bail; \
	} \
	*b = '\0'; \
} while (0)

#define NUMBER(x,a,s) do { \
	(x) = strtoul((a), &endptr, 10); \
	if (*endptr) { \
		logger(LOG_ERR, "failed to parse entry #%u from %s: '%s' does not look like a %s", line, file, (a), (s)); \
		goto bail; \
	} \
} while (0)

	if (db->dbs & AUTHDB_PASSWD) {
		file = string("%s/passwd",  db->root);
		io = fopen(file, "r"); free(file);
		if (!io) goto bail;

		line = 0;
		while ((fgets(LINE, LINEMAX - 1, io)) != NULL) {
			if ((a = strrchr(LINE, '\n')) != NULL) *a = '\0';
			line++; field = 0; a = b = LINE;

			FIELD("passwd", "username");
			user = user_find(db, a, -1);
			if (!user) {
				user = user_add(db);
				user->name = strdup(a);
			}
			user->state |= AUTHDB_PASSWD;

			a = ++b; FIELD("passwd", "cleartext password");
			user->clear_pass = strdup(a);

			a = ++b; FIELD("passwd", "UID number");
			NUMBER(user->uid, a, "valid UID");

			a = ++b; FIELD("passwd", "GID number");
			NUMBER(user->gid, a, "valid GID");

			a = ++b; FIELD("passwd", "comment");
			user->comment = strdup(a);

			a = ++b; FIELD("passwd", "home directory");
			user->home = strdup(a);

			/* last field */
			user->shell = strdup(++b);
		}

		fclose(io);
	}

	if (db->dbs & AUTHDB_SHADOW) {
		file = string("%s/shadow",  db->root);
		io = fopen(file, "r"); free(file);
		if (!io) goto bail;

		line = 0;
		while ((fgets(LINE, LINEMAX - 1, io)) != NULL) {
			if ((a = strrchr(LINE, '\n')) != NULL) *a = '\0';
			line++; field = 0; a = b = LINE;

			FIELD("shadow", "usernae");
			user = user_find(db, a, -1);
			if (!user) {
				user = user_add(db);
				user->name = strdup(a);
			}
			user->state |= AUTHDB_SHADOW;

			a = ++b; FIELD("shadow", "encrypted password");
			user->crypt_pass = strdup(a);

			a = ++b; FIELD("shadow", "date of last password change");
			NUMBER(user->creds.last_changed, a, "number");

			a = ++b; FIELD("shadow", "minimum password age");
			NUMBER(user->creds.min_days, a, "number");

			a = ++b; FIELD("shadow", "maximum password age");
			NUMBER(user->creds.max_days, a, "number");

			a = ++b; FIELD("shadow", "password warning period");
			NUMBER(user->creds.warn_days, a, "number");

			a = ++b; FIELD("shadow", "password inactivity period");
			if (!*a) user->creds.grace_period = -1;
			else NUMBER(user->creds.grace_period, a, "number");

			a = ++b; FIELD("shadow", "expiration date");
			if (!*a) user->creds.expiration = -1;
			else NUMBER(user->creds.expiration, a, "number");

			/* last field */
			a = ++b;
			NUMBER(user->creds.flags, a, "number");
		}

		fclose(io);
	}

	if (db->dbs & AUTHDB_GROUP) {
		file = string("%s/group",  db->root);
		io = fopen(file, "r"); free(file);
		if (!io) goto bail;

		line = 0;
		while ((fgets(LINE, LINEMAX - 1, io)) != NULL) {
			if ((a = strrchr(LINE, '\n')) != NULL) *a = '\0';
			line++; field = 0; a = b = LINE;

			FIELD("group", "group name");
			group = group_find(db, a, -1);
			if (!group) {
				group = group_add(db);
				group->name = strdup(a);
			}
			group->state |= AUTHDB_GROUP;

			a = ++b; FIELD("group", "cleartext password");
			group->clear_pass = strdup(a);

			a = ++b; FIELD("group", "GID number");
			NUMBER(group->gid, a, "valid GID");

			/* last field */
			group->raw_members = strdup(++b);
		}

		fclose(io);
	}

	if (db->dbs & AUTHDB_GSHADOW) {
		file = string("%s/gshadow",  db->root);
		io = fopen(file, "r"); free(file);
		if (!io) goto bail;

		line = 0;
		while ((fgets(LINE, LINEMAX - 1, io)) != NULL) {
			if ((a = strrchr(LINE, '\n')) != NULL) *a = '\0';
			line++; field = 0; a = b = LINE;

			FIELD("gshadow", "group name");
			group = group_find(db, a, -1);
			if (!group) {
				group = group_add(db);
				group->name = strdup(a);
			}
			group->state |= AUTHDB_GSHADOW;

			a = ++b; FIELD("gshadow","encrypted password");
			group->crypt_pass = strdup(a);

			a = ++b; FIELD("gshadow", "administrator list");
			group->raw_admins = strdup(a);

			/* last field */
			free(group->raw_members);
			group->raw_members = strdup(++b);
		}

		fclose(io);
	}

	/* set up membership / adminhood lists */
	for_each_object(group, &db->groups, l) {
		if (group->raw_members && *group->raw_members) {
			strings_t *lst = strings_split(group->raw_members,
					strlen(group->raw_members), ",", SPLIT_NORMAL);
			int i;
			for_each_string(lst,  i)
				s_add_member(db, GROUP_MEMBER, group, user_find(db, lst->strings[i], -1));
			strings_free(lst);
		}

		if (group->raw_admins && *group->raw_admins) {
			strings_t *lst = strings_split(group->raw_admins,
					strlen(group->raw_admins), ",", SPLIT_NORMAL);
			int i;
			for_each_string(lst,  i)
				s_add_member(db, GROUP_ADMIN, group, user_find(db, lst->strings[i], -1));
			strings_free(lst);
		}
	}

	return db;

bail:
	if (io) fclose(io);
	authdb_close(db);
	return NULL;

#undef FIELD
#undef NUMBER
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
		rc = cw_frename(tmpfile, file); free(file);
		if (rc != 0) {
			unlink(tmpfile); free(tmpfile);
			goto bail;
		}
		free(tmpfile);
	}

	if (db->dbs & AUTHDB_SHADOW) {
		file = string("%s/shadow", db->root);
		tmpfile = string("%s/.shadow.%x", db->root, rand());
		io = fopen(tmpfile, "w");
		if (!io) goto bail;

		for_each_object(user, &db->users, l) {
#define null_string(f,s) user->creds.f <= 0 ? string("") : string(s, user->creds.f)
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
		rc = cw_frename(tmpfile, file); free(file);
		if (rc != 0) {
			unlink(tmpfile); free(tmpfile);
			goto bail;
		}
		free(tmpfile);
	}

	if (db->dbs & AUTHDB_GROUP) {
		file = string("%s/group", db->root);
		tmpfile = string("%s/.group.%x", db->root, rand());

		io = fopen(tmpfile, "w");
		if (!io) goto bail;

		for_each_object(group, &db->groups, l) {
			/* re-construct raw_members from member list */
			free(group->raw_members);
			group->raw_members = s_group_list(group, GROUP_MEMBER);

			fprintf(io, "%s:%s:%u:%s\n",
				group->name, group->clear_pass, group->gid, group->raw_members);
		}

		fclose(io);
		rc = cw_frename(tmpfile, file); free(file);
		if (rc != 0) {
			unlink(tmpfile); free(tmpfile);
			goto bail;
		}
		free(tmpfile);
	}

	if (db->dbs & AUTHDB_GSHADOW) {
		file = string("%s/gshadow", db->root);
		tmpfile = string("%s/.gshadow.%x", db->root, rand());

		io = fopen(tmpfile, "w");
		if (!io) goto bail;

		for_each_object(group, &db->groups, l) {
			/* re-construct raw_admins from admin list */
			free(group->raw_admins);
			group->raw_admins = s_group_list(group, GROUP_ADMIN);

			/* re-construct raw_members from member list */
			free(group->raw_members);
			group->raw_members = s_group_list(group, GROUP_MEMBER);

			fprintf(io, "%s:%s:%s:%s\n",
				group->name, group->crypt_pass,
				group->raw_admins, group->raw_members);
		}

		fclose(io);
		rc = cw_frename(tmpfile, file); free(file);
		if (rc != 0) {
			unlink(tmpfile); free(tmpfile);
			goto bail;
		}
		free(tmpfile);
	}

	return 0;
bail:
	if (io) fclose(io);
	return 1;
}

void authdb_close(authdb_t *db)
{
	if (!db) return;
	user_t *user, *utmp;
	for_each_object_safe(user, utmp, &db->users, l) {
		list_delete(&user->l);
		free(user->name);
		free(user->clear_pass);
		free(user->crypt_pass);
		free(user->comment);
		free(user->home);
		free(user->shell);
		free(user);
	}

	group_t *group, *gtmp;
	for_each_object_safe(group, gtmp, &db->groups, l) {
		list_delete(&group->l);
		free(group->name);
		free(group->clear_pass);
		free(group->crypt_pass);
		free(group->raw_members);
		free(group->raw_admins);
		free(group);
	}

	member_t *member, *mtmp;
	for_each_object_safe(member, mtmp, &db->memberships, l) {
		list_delete(&member->l);
		free(member);
	}

	free(db->root);
	free(db);
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
	for_each_object(member, &user->member_of, on_user)
		if (member->group)
			strings_add(creds, member->group->name);

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
	user->db = db;
	list_push(&db->users, &user->l);
	list_init(&user->member_of);
	list_init(&user->admin_of);
	return user;
}

void user_remove(user_t *user)
{
	if (!user) return;
	list_delete(&user->l);

	member_t *member, *tmp;
	for_each_object_safe(member, tmp, &user->db->memberships, l)
		if (member->user == user)
			s_member_free(member);

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
	group->db = db;
	list_push(&db->groups, &group->l);
	list_init(&group->members);
	list_init(&group->admins);
	return group;
}

void group_remove(group_t *group)
{
	if (!group) return;
	list_delete(&group->l);

	member_t *member, *tmp;
	for_each_object_safe(member, tmp, &group->db->memberships, l)
		if (member->group == group)
			s_member_free(member);

	free(group->name);
	free(group->clear_pass);
	free(group->crypt_pass);
	free(group->raw_members);
	free(group->raw_admins);
	free(group);
}

int group_has(group_t *group, int type, user_t *user)
{
	if (!user) return 1;
	member_t *needle;
	list_t *list = (type == GROUP_MEMBER ? &group->members : &group->admins);
	for_each_object(needle, list, on_group)
		if (needle->user == user) return 0;
	return 1;
}

int group_join(group_t *group, int type, user_t *user)
{
	if (!user) return 1;
	s_add_member(group->db, type, group, user);

	member_t *needle;
	list_t *list = (type == GROUP_MEMBER ? &group->members : &group->admins);
	for_each_object(needle, list, on_group)
		if (needle->user == user) return 0;
	return 1;
}

int group_kick(group_t *group, int type, user_t *user)
{
	if (!user) return 1;
	s_remove_member(group->db, type, group, user);

	member_t *needle;
	list_t *list = (type == GROUP_MEMBER ? &group->members : &group->admins);
	for_each_object(needle, list, on_group)
		if (needle->user == user) return 1;
	return 0;
}
