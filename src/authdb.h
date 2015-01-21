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

#ifndef AUTHDB_H
#define AUTHDB_H

#include "clockwork.h"

#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <grp.h>

#define AUTHDB_PASSWD    0x01
#define AUTHDB_SHADOW    0x02
#define AUTHDB_GROUP     0x04
#define AUTHDB_GSHADOW   0x08

#define AUTHDB_ALL       0x0f

typedef struct {
	int    dbs;
	char  *root;

	list_t users;
	list_t groups;
	list_t memberships;
} authdb_t;

typedef struct {
	authdb_t *db;

	int    state;

	char  *name;
	uid_t  uid;
	gid_t  gid;
	char  *clear_pass;
	char  *crypt_pass;
	char  *comment;
	char  *home;
	char  *shell;

	struct {
		long last_changed;
		long min_days;
		long max_days;
		long warn_days;
		long grace_period;
		long expiration;
		unsigned long flags;
	} creds;

	list_t member_of;
	list_t admin_of;

	list_t l;
} user_t;

typedef struct {
	authdb_t *db;

	int    state;

	char  *name;
	gid_t  gid;
	char  *clear_pass;
	char  *crypt_pass;
	char  *raw_members;
	char  *raw_admins;

	list_t members;
	list_t admins;

	list_t l;
} group_t;

typedef struct {
	user_t  *user;
	group_t *group;
	uint8_t  refs;

	list_t   on_user;
	list_t   on_group;
	list_t   l;
} member_t;

authdb_t* authdb_read(const char *root, int dbs);
int authdb_write(authdb_t *db);
void authdb_close(authdb_t *db);

char* authdb_creds(authdb_t *db, const char *user);

uid_t authdb_nextuid(authdb_t *db, uid_t start);
gid_t authdb_nextgid(authdb_t *db, gid_t start);

#define NO_UID (uid_t)(-1)
user_t* user_find(authdb_t *db, const char *name, uid_t uid);
user_t* user_add(authdb_t *db);
void user_remove(user_t *user);

#define NO_GID (gid_t)(-1)
group_t* group_find(authdb_t *db, const char *name, gid_t gid);
group_t* group_add(authdb_t *db);
void group_remove(group_t *group);

#define GROUP_MEMBER 1
#define GROUP_ADMIN  2
int group_has(group_t *group, int type, user_t *user);
int group_join(group_t *group, int type, user_t *user);
int group_kick(group_t *group, int type, user_t *user);

#endif
