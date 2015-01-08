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

typedef struct {
	int    state;

	char  *name;
	uid_t  uid;
	gid_t  gid;
	char  *clear_pass;
	char  *crypt_pass;
	char  *comment;
	char  *home;
	char  *shell;
	char *tmpfile;

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
	int     id; /* uid or gid */
	char   *name;
	list_t  l;
} member_t;

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
} authdb_t;

authdb_t* authdb_read(const char *root, int dbs);
int authdb_write(authdb_t *db);
void authdb_close(authdb_t *db);

char* authdb_creds(authdb_t *db, const char *user);

uid_t authdb_nextuid(authdb_t *db, uid_t start);
gid_t authdb_nextgid(authdb_t *db, gid_t start);

user_t* user_find(authdb_t *db, const char *name, uid_t uid);
user_t* user_add(authdb_t *db);
void user_remove(user_t *user);

group_t* group_find(authdb_t *db, const char *name, gid_t gid);
group_t* group_add(authdb_t *db);
void group_remove(group_t *group);

#endif
