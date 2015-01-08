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

#include <unistd.h>

#include "test.h"
#include "../src/authdb.h"
#include "../src/userdb.h"

/*********************************************************/

/* test /etc/passwd file */
#define PWFILE      TEST_DATA "/userdb/passwd"

/* test /etc/group file */
#define GRFILE      TEST_DATA "/userdb/group"

/* test /etc/gshadow file */
#define SGFILE      TEST_DATA "/userdb/gshadow"

/*********************************************************/

TESTS {
	subtest {
		authdb_t *db;
		user_t *user;
		group_t *group;

		db = authdb_read("/path/to/nowhere", AUTHDB_ALL);
		is_null(db, "failed to read non-existent root path");

		db = authdb_read(TEST_DATA "/userdb", AUTHDB_ALL);
		isnt_null(db, "opened all four databases in " TEST_DATA "/userdb");

		// 'root' is the first entry
		user = user_find(db, "root", -1);
			isnt_null(user, "found root by username search");
			if (!user) break;
			is_int(user->state, AUTHDB_PASSWD | AUTHDB_SHADOW, "root is in both passwd and shadow");
			is_int(user->uid, 0, "root UID");
			is_int(user->gid, 0, "root GID");
			is_string(user->name, "root", "root username");
		user = user_find(db, NULL, 0);
			isnt_null(user, "lookup root by UID");
			if (!user) break;
			is_string(user->name, "root", "root username");

		// 'svc' is the last entry
		user = user_find(db, "svc", 0);
			isnt_null(user, "lookup svc by name");
			if (!user) break;
			is_int(user->uid, 999, "svc UID");
			is_int(user->gid, 909, "svc GID");
			is_string(user->name, "svc", "svc username");
		user = user_find(db, NULL, 999);
			isnt_null(user, "lookup svc by UID");
			if (!user) break;
			is_string(user->name, "svc", "svc username");

		// 'user' is a middle entry
		user = user_find(db, "user", 999); /* 999 will be ignored */
			isnt_null(user, "lookup user by name");
			if (!user) break;
			is_int(user->uid, 100, "user UID");
			is_int(user->gid,  20, "user GID");
			is_string(user->name, "user", "user username");
		user = user_find(db, NULL, 100);
			isnt_null(user, "lookup user by UID");
			if (!user) break;
			is_string(user->name, "user", "user username");

		// first group entry
		group = group_find(db, "root", -1);
			isnt_null(group, "found root group by name");
			if (!group) break;
			is_int(group->gid, 0, "root group GID");
			is_string(group->name, "root", "root group name");
		group = group_find(db, NULL, 0);
			isnt_null(group, "found root group by GID");
			if (!group) break;
			is_string(group->name, "root", "root group name");

		// last group entry
		group = group_find(db, "service", 0);
			isnt_null(group, "found service group by name");
			if (!group) break;
			is_int(group->gid, 909, "service group GID");
			is_string(group->name, "service", "service group name");
		group = group_find(db, NULL, 909);
			isnt_null(group, "found service group by GID");
			if (!group) break;
			is_string(group->name, "service", "service group name");

		// middle group entry
		group = group_find(db, "users", 909); // 909 will be ignored
			isnt_null(group, "found users group by name");
			if (!group) break;
			is_int(group->gid, 20, "users group GID");
			is_string(group->name, "users", "users group name");
		group = group_find(db, NULL, 20);
			isnt_null(group, "found users group by GID");
			if (!group) break;
			is_string(group->name, "users", "users group name");

		authdb_close(db);
	}

	subtest {
		authdb_t *db;
		user_t *user;
		group_t *group;

		db = authdb_read(TEST_DATA "/userdb", AUTHDB_ALL);
		is_null(user_find(db, "new_user", -1), "new_user does not exist yet");
		is_null(group_find(db, "new_group", -1), "new_group does not exist yet");

		user = user_add(db);
		isnt_null(user, "user_add() returned a new user");
		user->name = strdup("new_user");
		user->clear_pass = strdup("x");
		user->crypt_pass = strdup("$6$pwhash");
		user->uid        = 500;
		user->gid        = 500;
		user->comment    = strdup("New User,,,");
		user->home       = strdup("/home/new_user");
		user->shell      = strdup("/bin/bash");
		user->creds.last_changed = 14800;
		user->creds.min_days     = 1;
		user->creds.max_days     = 6;
		user->creds.warn_days    = 4;
		user->creds.grace_period = 0;
		user->creds.expiration   = 0;
		isnt_null(user_find(db, "new_user", -1), "new_user exists in memory");

		group = group_add(db);
		isnt_null(group, "group_add() returns a new group");
		group->name       = strdup("new_group");
		group->clear_pass = strdup("x");
		group->crypt_pass = strdup("$6$pwhash");
		group->gid        = 500;

		authdb_write(db);
		authdb_close(db);

		db = authdb_read(TEST_DATA "/userdb", AUTHDB_ALL);

		isnt_null(user = user_find(db, "new_user", -1), "new_user exists on disk");

		is_string( user->name,               "new_user",        "new user username");
		is_string( user->clear_pass,         "x",               "new user password field");
		is_string( user->crypt_pass,         "$6$pwhash",       "new user password hash");
		is_int(    user->uid,                500,               "new user UID");
		is_int(    user->gid,                500,               "new user GID");
		is_string( user->comment,            "New User,,,",     "new user GECOS/comment");
		is_string( user->home,                "/home/new_user", "new user home directory");
		is_string( user->shell,               "/bin/bash",      "new user shell");
		is_int(    user->creds.last_changed,  14800,            "last-changed value");
		is_int(    user->creds.min_days,      1,                "min password age");
		is_int(    user->creds.max_days,      6,                "max password age");
		is_int(    user->creds.warn_days,     4,                "pw change warning");
		is_int(    user->creds.grace_period,  0,                "account inactivity deadline");
		is_int(    user->creds.expiration,    0,                "account expiry");

		isnt_null(group = group_find(db, "new_group", -1), "new_group exists on disk");
		is_string(group->name,       "new_group", "group name");
		is_string(group->clear_pass, "x",         "group name");
		is_string(group->crypt_pass, "$6$pwhash", "group name");
		is_int(   group->gid,        500,         "group GID");

		authdb_close(db);
	}

	subtest {
		authdb_t *db;
		user_t *user;
		group_t *group;

		db = authdb_read(TEST_DATA "/userdb", AUTHDB_ALL);

		user = user_find(db, "sys", -1);
		isnt_null(user, "found 'sys' user (for deletion)");
		user_remove(user);
		is_null(user_find(db, "sys", -1), "'sys' user no longer in memory");

		group = group_find(db, "sys", -1);
		isnt_null(group, "found 'sys' group (for deletion)");
		group_remove(group);
		is_null(group_find(db, "sys", -1), "'sys' group no longer in memory");

		authdb_write(db);
		db = authdb_read(TEST_DATA "/userdb", AUTHDB_ALL);

		is_null(user_find(db, "sys", -1), "'sys' user no longer on disk");
		is_null(group_find(db, "sys", -1), "'sys' group no longer on disk");
	}

	subtest {
		authdb_t *db = authdb_read(TEST_DATA "/userdb", AUTHDB_ALL);
		is_int(authdb_nextuid(db, 1000), 8192, "Next UUID == max(UID)+1");
		authdb_close(db);
	}

	subtest {
		struct grdb *db;
		struct group *gr;

		isnt_null(db = grdb_init(GRFILE), "read group db");
		isnt_null(gr = grdb_get_by_name(db, "members"), "found 'members' group");
		isnt_null(gr->gr_mem, "'members' group has a gr_mem array");

		is_string(gr->gr_mem[0], "account1", "gr_mem[0]");
		is_string(gr->gr_mem[1], "account2", "gr_mem[1]");
		is_string(gr->gr_mem[2], "account3", "gr_mem[2]");
		is_null(gr->gr_mem[3], "no more members");

		grdb_free(db);
	}

	subtest {
		struct sgdb *db;
		struct sgrp *sg;

		isnt_null(db = sgdb_init(SGFILE), "read gshadow db");
		isnt_null(sg = sgdb_get_by_name(db, "members"), "found the 'members' gshadow entry");

		isnt_null(sg->sg_mem, "'members' gshadow entry has list of members");
		is_string(sg->sg_mem[0], "account1", "sg_mem[0]");
		is_string(sg->sg_mem[1], "account2", "sg_mem[1]");
		is_string(sg->sg_mem[2], "account3", "sg_mem[2]");
		is_null(sg->sg_mem[3], "no more members");

		isnt_null(sg->sg_adm, "'members' gshadow entry has list of admins");
		is_string(sg->sg_adm[0], "admin1", "sg_adm[0]");
		is_string(sg->sg_adm[1], "admin2", "sg_adm[1]");
		is_null(sg->sg_adm[2], "no more admins");

		sgdb_free(db);
	}

	subtest {
		authdb_t *db = authdb_read(TEST_DATA "/userdb", AUTHDB_ALL);

		char *s = authdb_creds(db, "account1");
		is_string(s, "account1:users:members:service",
				"Generated user and groups list");
		free(s);

		s = authdb_creds(db, "account2");
		is_null(s, "No records for account2");

		authdb_close(db);
	}

	done_testing();
}
