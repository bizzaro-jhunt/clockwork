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

static void reset(const char *root)
{
	FILE *io;
	char *file;

	mkdir(root, 0755);
	file = string("%s/passwd", root); io = fopen(file, "w"); free(file);
	if (!io) BAIL_OUT("test setup: failed to open {root}/passwd database for reset");
	fprintf(io, "root:x:0:0:root:/root:/bin/bash\n");
	fprintf(io, "daemon:x:1:1:daemon:/usr/sbin:/bin/sh\n");
	fprintf(io, "bin:x:2:2:bin:/bin:/bin/sh\n");
	fprintf(io, "sys:x:3:3:sys:/dev:/bin/sh\n");
	fprintf(io, "user:x:100:20:User Account,,,:/home/user:/bin/bash\n");
	fprintf(io, "account1:x:901:20:Account One:/home/acct1:/sbin/nologin\n");
	fprintf(io, "account2:x:902:20:Account Two:/home/acct2:/sbin/nologin\n");
	fprintf(io, "account3:x:903:20:Account Three:/home/acct3:/sbin/nologin\n");
	fprintf(io, "account4:x:904:20:Account Four:/home/acct4:/sbin/nologin\n");
	fprintf(io, "admin1:x:911:20:Admin One:/home/adm1:/sbin/nologin\n");
	fprintf(io, "admin2:x:912:20:Admin Two:/home/adm2:/sbin/nologin\n");
	fprintf(io, "admin3:x:913:20:Admin Three:/home/adm3:/sbin/nologin\n");
	fprintf(io, "svc:x:999:909:service account:/tmp/nonexistent:/sbin/nologin\n");
	if (is_root()) fchmod(fileno(io), 0644);
	fclose(io);

	file = string("%s/shadow", root); io = fopen(file, "w"); free(file);
	if (!io) BAIL_OUT("test setup: failed to open {root}/shadow database for reset");
	fprintf(io, "root:!:14009:0:99999:7:::\n");
	fprintf(io, "daemon:*:13991:0:99999:7:::\n");
	fprintf(io, "bin:*:13991:0:99999:7:::\n");
	fprintf(io, "sys:*:13991:0:99999:7:::\n");
	fprintf(io, "user:$6$nahablHe$1qen4PePmYtEIC6aCTYoQFLgMp//snQY7nDGU7.9iVzXrmmCYLDsOKc22J6MPRUuH/X4XJ7w.JaEXjofw9h1d/:14871:0:99999:7:::\n");
	fprintf(io, "svc:*:13991:0:99999:7:::\n");
	if (is_root()) fchmod(fileno(io), 0600);
	fclose(io);

	file = string("%s/group", root); io = fopen(file, "w"); free(file);
	if (!io) BAIL_OUT("test setup: failed to open {root}/group database for reset");
	fprintf(io, "root:x:0:\n");
	fprintf(io, "daemon:x:1:\n");
	fprintf(io, "bin:x:2:\n");
	fprintf(io, "sys:x:3:\n");
	fprintf(io, "members:x:4:account1,account2,account3\n");
	fprintf(io, "users:x:20:\n");
	fprintf(io, "service:x:909:account1,account2\n");
	if (is_root()) fchmod(fileno(io), 0644);
	fclose(io);

	file = string("%s/gshadow", root); io = fopen(file, "w"); free(file);
	if (!io) BAIL_OUT("test setup: failed to open {root}/gshadow database for reset");
	fprintf(io, "root:*::\n");
	fprintf(io, "daemon:*::\n");
	fprintf(io, "bin:*::\n");
	fprintf(io, "sys:*::\n");
	fprintf(io, "members:*:admin1,admin2:account1,account2,account3\n");
	fprintf(io, "users:*::\n");
	fprintf(io, "service:!:admin2:account1,account2\n");
	if (is_root()) fchmod(fileno(io), 0600);
	fclose(io);
}

TESTS {
	if (is_root()) {
		subtest { /* verify UID/GID/mode */
			reset("t/tmp");

			authdb_t *db;
			db = authdb_read("t/tmp", AUTHDB_ALL);
			isnt_null(db, "opened all four databases in t/tmp");
			authdb_write(db);
			authdb_close(db);

			struct stat st;
			is_int(stat("t/tmp/passwd", &st), 0, "stat'd t/tmp/passwd");
			is_int(st.st_uid,        0, "t/tmp/passwd is owned by root (0)");
			is_int(st.st_gid,        0, "t/tmp/passwd is owned by root (0)");
			is_int(st.st_mode, 0100644, "t/tmp/passwd has 0644 permissions");

			is_int(stat("t/tmp/shadow", &st), 0, "stat'd t/tmp/shadow");
			is_int(st.st_uid,        0, "t/tmp/shadow is owned by root (0)");
			is_int(st.st_gid,        0, "t/tmp/shadow is owned by root (0)");
			is_int(st.st_mode, 0100600, "t/tmp/shadow has 0600 permissions");

			is_int(stat("t/tmp/group", &st), 0, "stat'd t/tmp/group");
			is_int(st.st_uid,        0, "t/tmp/group is owned by root (0)");
			is_int(st.st_gid,        0, "t/tmp/group is owned by root (0)");
			is_int(st.st_mode, 0100644, "t/tmp/group has 0644 permissions");

			is_int(stat("t/tmp/gshadow", &st), 0, "stat'd t/tmp/gshadow");
			is_int(st.st_uid,        0, "t/tmp/gshadow is owned by root (0)");
			is_int(st.st_gid,        0, "t/tmp/gshadow is owned by root (0)");
			is_int(st.st_mode, 0100600, "t/tmp/gshadow has 0600 permissions");
		}
	}

	subtest {
		reset("t/tmp");

		authdb_t *db;
		user_t *user;
		group_t *group;

		db = authdb_read("/path/to/nowhere", AUTHDB_ALL);
		is_null(db, "failed to read non-existent root path");

		db = authdb_read("t/tmp", AUTHDB_ALL);
		isnt_null(db, "opened all four databases in " "t/tmp");

		// 'root' is the first entry
		user = user_find(db, "root", NO_UID);
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
		group = group_find(db, "root", NO_GID);
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
		reset("t/tmp");

		authdb_t *db;
		user_t *user;
		group_t *group;

		db = authdb_read("t/tmp", AUTHDB_ALL);
		is_null(user_find(db, "new_user", NO_UID), "new_user does not exist yet");
		is_null(group_find(db, "new_group", NO_GID), "new_group does not exist yet");

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
		isnt_null(user_find(db, "new_user", NO_UID), "new_user exists in memory");

		group = group_add(db);
		isnt_null(group, "group_add() returns a new group");
		group->name       = strdup("new_group");
		group->clear_pass = strdup("x");
		group->crypt_pass = strdup("$6$pwhash");
		group->gid        = 500;

		authdb_write(db);
		authdb_close(db);

		db = authdb_read("t/tmp", AUTHDB_ALL);

		isnt_null(user = user_find(db, "new_user", NO_UID), "new_user exists on disk");

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
		is_int(    user->creds.grace_period,  -1,               "account inactivity deadline");
		is_int(    user->creds.expiration,    -1,               "account expiry");

		isnt_null(group = group_find(db, "new_group", NO_GID), "new_group exists on disk");
		is_string(group->name,       "new_group", "group name");
		is_string(group->clear_pass, "x",         "group name");
		is_string(group->crypt_pass, "$6$pwhash", "group name");
		is_int(   group->gid,        500,         "group GID");

		authdb_close(db);
	}

	subtest {
		reset("t/tmp");

		authdb_t *db;
		user_t *user;
		group_t *group;

		db = authdb_read("t/tmp", AUTHDB_ALL);

		user = user_find(db, "sys", NO_UID);
		isnt_null(user, "found 'sys' user (for deletion)");
		user_remove(user);
		is_null(user_find(db, "sys", NO_UID), "'sys' user no longer in memory");

		group = group_find(db, "sys", NO_GID);
		isnt_null(group, "found 'sys' group (for deletion)");
		group_remove(group);
		is_null(group_find(db, "sys", NO_GID), "'sys' group no longer in memory");

		authdb_write(db);
		authdb_close(db);

		db = authdb_read("t/tmp", AUTHDB_ALL);

		is_null(user_find(db, "sys", NO_UID), "'sys' user no longer on disk");
		is_null(group_find(db, "sys", NO_GID), "'sys' group no longer on disk");

		authdb_write(db);
		authdb_close(db);
	}

	subtest {
		reset("t/tmp");

		authdb_t *db = authdb_read("t/tmp", AUTHDB_ALL);
		is_int(authdb_nextuid(db, 800), 800, "nextuid finds first unused uid");
		is_int(authdb_nextuid(db,   1),   4, "nextuid finds holes");
		is_int(authdb_nextgid(db, 900), 900, "nextgid finds first unused gid");
		is_int(authdb_nextgid(db,   1),   5, "nextgid finds holes");
		authdb_close(db);
	}

	subtest {
		reset("t/tmp");

		authdb_t *db = authdb_read("t/tmp", AUTHDB_ALL);
		group_t *group = group_find(db, "members", NO_GID);
		isnt_null(group, "found 'members' group for membership test");

		static const char *members[] = { "account1", "account2", "account3", NULL };
		static const char *admins[] = { "admin1", "admin2", NULL };
		size_t i; member_t *member;

		i = 0;
		for_each_object(member, &group->members, on_group) {
			if (!members[i]) BAIL_OUT("ran out of group members to expect!");
			isnt_null(member->user, "membership record %i has a user object", i);
			is_string(member->user->name, members[i], "membership record %i == %s", i, members[i]);
			i++;
		}
		is_null(members[i], "saw all of the expected group members");

		i = 0;
		for_each_object(member, &group->admins, on_group) {
			if (!admins[i]) BAIL_OUT("ran out of group admins to expect!");
			isnt_null(member->user, "adminhood record %i has a user object", i);
			is_string(member->user->name, admins[i], "adminhood record %i == %s", i, members[i]);
			i++;
		}
		is_null(admins[i], "saw all of the expected group admins");

		authdb_close(db);
	}

	subtest {
		reset("t/tmp");

		authdb_t *db = authdb_read("t/tmp", AUTHDB_ALL);
		group_t *group = group_find(db, "members", NO_GID);
		isnt_null(group, "found 'members' group for membership test");

		user_t *user;
		user = user_find(db, "account2", NO_UID);
		isnt_null(user, "found 'account2' in database");
		ok(group_has(group, GROUP_MEMBER, user) == 0, "account2 is a member of group 'members'");
		ok(group_has(group, GROUP_ADMIN,  user) != 0, "account2 is not an admin of 'members'");

		ok(group_kick(group, GROUP_MEMBER, user) == 0, "kicked account2 from 'members'");
		ok(group_has(group, GROUP_MEMBER, user)  != 0, "account2 no longer in 'members'");
		ok(group_has(group, GROUP_ADMIN,  user)  != 0, "account2 is not an admin of 'members'");

		ok(group_join(group, GROUP_ADMIN,  user) == 0, "joined account2 to 'members' (normal)");
		ok(group_join(group, GROUP_MEMBER, user) == 0, "joined account2 to 'members' (admin)");
		ok(group_has(group, GROUP_MEMBER, user)  == 0, "account2 is back in 'members'");
		ok(group_has(group, GROUP_ADMIN,  user)  == 0, "account2 is NOW an admin of 'members'");

		user = user_find(db, "account3", NO_UID);
		isnt_null(user, "found 'account3' in database");
		ok(group_has(group, GROUP_MEMBER, user) == 0, "account3 is a member of group 'members'");
		ok(group_has(group, GROUP_ADMIN,  user) != 0, "account3 is not an admin of 'members'");

		ok(group_kick(group, GROUP_MEMBER, user) == 0, "kicked account3 from 'members'");
		ok(group_has(group, GROUP_MEMBER, user)  != 0, "account3 no longer in 'members'");

		user = user_find(db, "admin1", NO_UID);
		isnt_null(user, "found 'admin2' in database");
		ok(group_has(group, GROUP_ADMIN,  user) == 0, "admin1 is an admin of group 'members'");
		ok(group_has(group, GROUP_MEMBER, user) != 0, "admin1 is not a member of 'members'");

		ok(group_has(group, GROUP_ADMIN,  NULL) != 0,
			"group_has() can handle NULL member searches");
		ok(group_has(group, GROUP_MEMBER, NULL) != 0,
			"group_has() can handle NULL admin searches");

		ok(group_kick(group, GROUP_MEMBER, NULL) != 0,
			"group_has() can handle NULL member removal");
		ok(group_kick(group, GROUP_ADMIN, NULL) != 0,
			"group_has() can handle NULL admin removal");

		ok(group_join(group, GROUP_MEMBER, NULL) != 0,
			"group_has() can handle NULL member joins");
		ok(group_join(group, GROUP_ADMIN, NULL) != 0,
			"group_has() can handle NULL admin joins");

		authdb_write(db);
		authdb_close(db);

		db = authdb_read("t/tmp", AUTHDB_ALL);
		group = group_find(db, "members", NO_GID);
		isnt_null(group, "found 'members' group for membership change verification");

		user = user_find(db, "account2", NO_UID);
		isnt_null(user, "found 'account2' in database");
		ok(group_has(group, GROUP_MEMBER, user)  == 0, "account2 is in 'members'");
		ok(group_has(group, GROUP_ADMIN,  user)  == 0, "account2 is an admin of 'members'");

		user = user_find(db, "account3", NO_UID);
		isnt_null(user, "found 'account3' in database");
		ok(group_has(group, GROUP_MEMBER, user) != 0, "account3 is not in 'members'");
		ok(group_has(group, GROUP_ADMIN,  user) != 0, "account3 is not an admin of 'members'");

		user = user_find(db, "admin1", NO_UID);
		isnt_null(user, "found 'admin2' in database");
		ok(group_has(group, GROUP_ADMIN,  user) == 0, "admin1 is an admin of group 'members'");
		ok(group_has(group, GROUP_MEMBER, user) != 0, "admin1 is not a member of 'members'");

		authdb_close(db);
	}

	subtest {
		reset("t/tmp");

		authdb_t *db = authdb_read("t/tmp", AUTHDB_ALL);

		char *s = authdb_creds(db, "account1");
		is_string(s, "account1:users:members:service",
				"Generated user and groups list");
		free(s);

		s = authdb_creds(db, "enoent");
		is_null(s, "no record for enoent");

		authdb_close(db);
	}

	subtest {
		reset("t/tmp");
		FILE *io;

		/* build some seriously screwed up authdb files */
		io = fopen("t/tmp/passwd", "w");
		if (!io) BAIL_OUT("test setup: failed to open t/tmp/passwd database for reset");
		fprintf(io, "root:x:0:0:root:/root:/bin/bash\n");
		fprintf(io, "daemon:x:1:1:daemon:/usr/sbin:/bin/sh\n");
		fclose(io);

		io = fopen("t/tmp/shadow", "w");
		if (!io) BAIL_OUT("test setup: failed to open t/tmp/shadow database for reset");
		fprintf(io, "root:!:14009:0:99999:7:::\n");
		fprintf(io, "daemon:*:13991:0:99999:7:::\n");
		fclose(io);

		io = fopen("t/tmp/group", "w");
		if (!io) BAIL_OUT("test setup: failed to open t/tmp/group database for reset");
		fprintf(io, "members:x:4:bad,accounts\n");
		fclose(io);

		io = fopen("t/tmp/gshadow", "w");
		if (!io) BAIL_OUT("test setup: failed to open t/tmp/gshadow database for reset");
		fprintf(io, "members:*:more,bad:accounts\n");
		fclose(io);

		authdb_t *db = authdb_read("t/tmp", AUTHDB_ALL);
		authdb_write(db);
		ok(1, "authdb_write() with bad group memberships (dangling accounts) doesn't crash");
		authdb_close(db);

		char buf[8192];
		io = fopen("t/tmp/group", "r");
		if (!io) BAIL_OUT("test setup: failed to open t/tmp/group database for confirmation");
		isnt_null(fgets(buf, 8192, io), "read a line from t/tmp/group");
		is(buf, "members:x:4:\n", "bad members removed from t/tmp/group");
		fclose(io);

		io = fopen("t/tmp/gshadow", "r");
		if (!io) BAIL_OUT("test setup: failed to open t/tmp/gshadow database for confirmation");
		isnt_null(fgets(buf, 8192, io), "read a line from t/tmp/gshadow");
		is(buf, "members:*::\n", "bad members removed from t/tmp/gshadow");
		fclose(io);
	}

	subtest {
		reset("t/tmp");
		authdb_t *db = authdb_read("t/tmp", AUTHDB_ALL);
		group_t *group = group_find(db, "members", NO_GID);
		isnt_null(group, "found 'members' group for membership test");
		group_remove(group);
		is_null(group_find(db, "members", NO_GID), "'members' group no longer in memory");

		user_t *user;
		user = user_find(db, "account2", NO_UID);
		isnt_null(user, "found 'account2' in database");
		user_remove(user);
		is_null(user_find(db, "account2", NO_UID), "'account2' user no longer in memory");

		authdb_write(db);
		ok(1, "authdb_write() doesn't crash when removing users / groups");
		authdb_close(db);
	}

	done_testing();
}
