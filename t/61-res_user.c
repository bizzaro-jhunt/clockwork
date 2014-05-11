/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

#include "test.h"
#include "../src/clockwork.h"
#include "../src/resources.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PASSWD_DB  TEST_DATA "/userdb/passwd"
#define SHADOW_DB  TEST_DATA "/userdb/shadow"

#define NEW_PASSWD_DB  TEST_TMP "/passwd"
#define NEW_SHADOW_DB  TEST_TMP "/shadow"

TESTS {
	subtest {
		struct res_user *user;
		char *key;

		isnt_null(user = res_user_new("user-key"), "generated user");
		isnt_null(key = res_user_key(user), "got user key");
		is_string(key, "user:user-key", "user key");
		free(key);
		res_user_free(user);
	}

	subtest {
		ok(res_user_notify(NULL, NULL) == 0, "res_user_notify is a NOOP");
		ok(res_user_norm(NULL, NULL, NULL) == 0, "res_user_norm is a NOOP");
	}

	subtest {
		res_user_free(NULL);
		pass("res_user_free(NULL) doesn't segfault");
	}

	subtest {
		struct res_user *ru;

		isnt_null(ru = res_user_new("user1"), "generated test user");

		ok( ENFORCED(ru, RES_USER_NAME),   "NAME enforced");
		ok(!ENFORCED(ru, RES_USER_PASSWD), "PASSWD not enforced");
		ok(!ENFORCED(ru, RES_USER_UID),    "UID not enforced");
		ok(!ENFORCED(ru, RES_USER_GID),    "GID not enforced");

		res_user_set(ru, "username", "Name");
		ok(ENFORCED(ru, RES_USER_NAME), "NAME enforced");
		is_string(ru->name, "Name", "username to enforce");

		res_user_set(ru, "pwhash", "!@#hash!@#");
		res_user_set(ru, "changepw", "yes");
		ok(ENFORCED(ru, RES_USER_PASSWD), "PASSWD enforced");
		is_string(ru->passwd, "!@#hash!@#", "passwd to enforce");

		res_user_set(ru, "uid", "4");
		ok(ENFORCED(ru, RES_USER_UID), "UID enforced");
		is_int(ru->uid, 4, "UID to enforce");

		res_user_set(ru, "gid", "5");
		ok(ENFORCED(ru, RES_USER_GID), "GID enforced");
		is_int(ru->gid, 5, "GID to enforce");

		res_user_set(ru, "comment", "GECOS comment");
		ok(ENFORCED(ru, RES_USER_GECOS), "GECOS enforced");
		is_string(ru->gecos, "GECOS comment", "comment to enforce");

		res_user_set(ru, "home", "/home/user");
		ok(ENFORCED(ru, RES_USER_DIR), "DIR enforced");
		is_string(ru->dir, "/home/user", "home dir to enforce");

		res_user_set(ru, "shell", "/bin/bash");
		ok(ENFORCED(ru, RES_USER_SHELL), "SHELL enforced");
		is_string(ru->shell, "/bin/bash", "shell to enforce");

		res_user_set(ru, "makehome", "/etc/skel.admin");
		ok(ENFORCED(ru, RES_USER_MKHOME), "MKHOME enforced");
		is_string(ru->skel, "/etc/skel.admin", "skeleton home to enforce");
		is_int(ru->mkhome, 1, "flag to prompt homedir creation");

		res_user_set(ru, "makehome", "no");
		ok(ENFORCED(ru, RES_USER_MKHOME), "MKHOME re-enforced");
		is_int(ru->mkhome, 0, "flag to prompt homedir creation");
		is_null(ru->skel, "skeleton directory");

		res_user_set(ru, "pwmin", "7");
		ok(ENFORCED(ru, RES_USER_PWMIN), "PWMIN enforced");
		is_int(ru->pwmin, 7, "min password age to enforce");

		res_user_set(ru, "pwmax", "45");
		ok(ENFORCED(ru, RES_USER_PWMAX), "PWMAX enforced");
		is_int(ru->pwmax, 45, "max password age to enforce");

		res_user_set(ru, "pwwarn", "29");
		ok(ENFORCED(ru, RES_USER_PWWARN), "PWWARN enforced");
		is_int(ru->pwwarn, 29, "password change warning to enforce");

		res_user_set(ru, "inact", "999");
		ok(ENFORCED(ru, RES_USER_INACT), "INACT enforced");
		is_int(ru->inact, 999, "account inactivty deadline to enforce");

		res_user_set(ru, "expiry", "100");
		ok(ENFORCED(ru, RES_USER_EXPIRE), "EXPIRE enforced");
		is_int(ru->expire, 100, "account expiry to enforce");

		res_user_set(ru, "locked", "yes");
		ok(ENFORCED(ru, RES_USER_LOCK), "LOCK enforced");
		is_int(ru->lock, 1, "account-locked flag");

		is_int(res_user_set(ru, "what-does-the-fox-say", "ring-ding-ring-ding"),
			-1, "res_user_set doesn't like nonsensical attributes");

		res_user_free(ru);
	}

	subtest {
		struct res_user *ru;

		ru = res_user_new("key");
		res_user_set(ru, "username", "user42");
		res_user_set(ru, "uid",      "123"); /* hex: 0000007b */
		res_user_set(ru, "gid",      "999"); /* hex: 000003e7 */
		res_user_set(ru, "home",     "/home/user");
		res_user_set(ru, "password", "sooper.seecret");

		ok(res_user_match(ru, "username", "user42")     == 0, "match username=user42");
		ok(res_user_match(ru, "uid",      "123")        == 0, "match uid=123");
		ok(res_user_match(ru, "gid",      "999")        == 0, "match gid=999");
		ok(res_user_match(ru, "home",     "/home/user") == 0, "match home=/home/user");

		ok(res_user_match(ru, "username", "bob")        != 0, "!match username=bob");
		ok(res_user_match(ru, "uid",      "42")         != 0, "!match uid=42");
		ok(res_user_match(ru, "gid",      "16")         != 0, "!match gid=16");
		ok(res_user_match(ru, "home",     "/root")      != 0, "!match home=/root");

		ok(res_user_match(ru, "password", "sooper.seecret") != 0,
				"password is not a matchable attr");

		res_user_free(ru);
	}

	subtest {
		struct res_user *ru;
		struct hash *h;

		isnt_null(ru = res_user_new("user"), "res_user created");

		isnt_null(h = hash_new(), "hash created");
		ok(res_user_attrs(ru, h) == 0, "got raw user attributes");
		is_string(hash_get(h, "username"), "user", "h.username");
		is_string(hash_get(h, "present"),  "yes",  "h.present"); // default
		is_null(hash_get(h, "uid"), "h.uid is unset");
		is_null(hash_get(h, "gid"), "h.gid is unset");
		is_null(hash_get(h, "home"), "h.home is unset");
		is_null(hash_get(h, "locked"), "h.locked is unset");
		is_null(hash_get(h, "comment"), "h.comment is unset");
		is_null(hash_get(h, "shell"), "h.shell is unset");
		is_null(hash_get(h, "password"), "h.password is unset");
		is_null(hash_get(h, "pwmin"), "h.pwmin is unset");
		is_null(hash_get(h, "pwmax"), "h.pwmax is unset");
		is_null(hash_get(h, "pwwarn"), "h.pwwarn is unset");
		is_null(hash_get(h, "inact"), "h.inact is unset");
		is_null(hash_get(h, "expiration"), "h.expiration is unset");
		is_null(hash_get(h, "skeleton"), "h.skeleton is unset");

		res_user_set(ru, "uid",        "1001");
		res_user_set(ru, "gid",        "2002");
		res_user_set(ru, "home",       "/home/user");
		res_user_set(ru, "locked",     "yes");
		res_user_set(ru, "comment",    "User");
		res_user_set(ru, "shell",      "/bin/bash");
		res_user_set(ru, "password",   "secret");
		res_user_set(ru, "changepw",   "yes");
		res_user_set(ru, "pwmin",      "2");
		res_user_set(ru, "pwmax",      "30");
		res_user_set(ru, "pwwarn",     "7");
		res_user_set(ru, "inact",      "14");
		res_user_set(ru, "expiration", "365");
		res_user_set(ru, "skeleton",   "/etc/skel");
		res_user_set(ru, "present",    "no");

		hash_free_all(h);
		isnt_null(h = hash_new(), "hash created");

		ok(res_user_attrs(ru, h) == 0, "got raw user attributes");
		is_string(hash_get(h, "uid"),        "1001",       "h.uid");
		is_string(hash_get(h, "gid"),        "2002",       "h.gid");
		is_string(hash_get(h, "home"),       "/home/user", "h.home");
		is_string(hash_get(h, "locked"),     "yes",        "h.locked");
		is_string(hash_get(h, "comment"),    "User",       "h.comment");
		is_string(hash_get(h, "shell"),      "/bin/bash",  "h.shell");
		is_string(hash_get(h, "password"),   "secret",     "h.password");
		is_string(hash_get(h, "pwmin"),      "2",          "h.pwmin");
		is_string(hash_get(h, "pwmax"),      "30",         "h.pwmax");
		is_string(hash_get(h, "pwwarn"),     "7",          "h.pwwarn");
		is_string(hash_get(h, "inact"),      "14",         "h.inact");
		is_string(hash_get(h, "expiration"), "365",        "h.expiration");
		is_string(hash_get(h, "skeleton"),   "/etc/skel",  "h.skeleton");
		is_string(hash_get(h, "present"),    "no",         "h.present");

		hash_free_all(h);
		isnt_null(h = hash_new(), "hash created");

		res_user_set(ru, "locked", "no");
		ok(res_user_attrs(ru, h) == 0, "got raw user attributes");
		is_string(hash_get(h, "locked"), "no", "h.locked");

		hash_free_all(h);
		res_user_free(ru);
	}

	done_testing();
}
