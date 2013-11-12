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

#include "test.h"
#include "../src/clockwork.h"
#include "../src/resources.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define PASSWD_DB  "t/data/userdb/passwd"
#define SHADOW_DB  "t/data/userdb/shadow"

#define NEW_PASSWD_DB  "t/tmp/passwd"
#define NEW_SHADOW_DB  "t/tmp/shadow"

int main(void) {
	test();

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
		is_string(ru->ru_name, "Name", "username to enforce");

		res_user_set(ru, "pwhash", "!@#hash!@#");
		res_user_set(ru, "changepw", "yes");
		ok(ENFORCED(ru, RES_USER_PASSWD), "PASSWD enforced");
		is_string(ru->ru_passwd, "!@#hash!@#", "passwd to enforce");

		res_user_set(ru, "uid", "4");
		ok(ENFORCED(ru, RES_USER_UID), "UID enforced");
		is_int(ru->ru_uid, 4, "UID to enforce");

		res_user_set(ru, "gid", "5");
		ok(ENFORCED(ru, RES_USER_GID), "GID enforced");
		is_int(ru->ru_gid, 5, "GID to enforce");

		res_user_set(ru, "comment", "GECOS comment");
		ok(ENFORCED(ru, RES_USER_GECOS), "GECOS enforced");
		is_string(ru->ru_gecos, "GECOS comment", "comment to enforce");

		res_user_set(ru, "home", "/home/user");
		ok(ENFORCED(ru, RES_USER_DIR), "DIR enforced");
		is_string(ru->ru_dir, "/home/user", "home dir to enforce");

		res_user_set(ru, "shell", "/bin/bash");
		ok(ENFORCED(ru, RES_USER_SHELL), "SHELL enforced");
		is_string(ru->ru_shell, "/bin/bash", "shell to enforce");

		res_user_set(ru, "makehome", "/etc/skel.admin");
		ok(ENFORCED(ru, RES_USER_MKHOME), "MKHOME enforced");
		is_string(ru->ru_skel, "/etc/skel.admin", "skeleton home to enforce");
		is_int(ru->ru_mkhome, 1, "flag to prompt homedir creation");

		res_user_set(ru, "makehome", "no");
		ok(ENFORCED(ru, RES_USER_MKHOME), "MKHOME re-enforced");
		is_int(ru->ru_mkhome, 0, "flag to prompt homedir creation");
		is_null(ru->ru_skel, "skeleton directory");

		res_user_set(ru, "pwmin", "7");
		ok(ENFORCED(ru, RES_USER_PWMIN), "PWMIN enforced");
		is_int(ru->ru_pwmin, 7, "min password age to enforce");

		res_user_set(ru, "pwmax", "45");
		ok(ENFORCED(ru, RES_USER_PWMAX), "PWMAX enforced");
		is_int(ru->ru_pwmax, 45, "max password age to enforce");

		res_user_set(ru, "pwwarn", "29");
		ok(ENFORCED(ru, RES_USER_PWWARN), "PWWARN enforced");
		is_int(ru->ru_pwwarn, 29, "password change warning to enforce");

		res_user_set(ru, "inact", "999");
		ok(ENFORCED(ru, RES_USER_INACT), "INACT enforced");
		is_int(ru->ru_inact, 999, "account inactivty deadline to enforce");

		res_user_set(ru, "expiry", "100");
		ok(ENFORCED(ru, RES_USER_EXPIRE), "EXPIRE enforced");
		is_int(ru->ru_expire, 100, "account expiry to enforce");

		res_user_set(ru, "locked", "yes");
		ok(ENFORCED(ru, RES_USER_LOCK), "LOCK enforced");
		is_int(ru->ru_lock, 1, "account-locked flag");

		is_int(res_user_set(ru, "what-does-the-fox-say", "ring-ding-ring-ding"),
			-1, "res_user_set doesn't like nonsensical attributes");

		res_user_free(ru);
	}

	subtest {
		struct res_user *ru;
		struct resource_env env;

		ru = res_user_new("svc");
		res_user_set(ru, "uid",      "7001");
		res_user_set(ru, "gid",      "8001");
		res_user_set(ru, "comment",  "SVC service account");
		res_user_set(ru, "shell",    "/sbin/nologin");
		res_user_set(ru, "home",     "/tmp/nonexistent");
		res_user_set(ru, "makehome", "yes");
		res_user_set(ru, "pwmin",    "4");
		res_user_set(ru, "pwmax",    "45");
		res_user_set(ru, "pwwarn",   "3");

		env.user_pwdb = pwdb_init(PASSWD_DB);
		if (!env.user_pwdb) bail("failed to read passwd database");

		env.user_spdb = spdb_init(SHADOW_DB);
		if (!env.user_spdb) bail("failed to read shadow database");

		ok(res_user_stat(ru, &env) == 0, "res_user_stat succeeds");
		ok(!DIFFERENT(ru, RES_USER_NAME),   "NAME is in compliance");
		ok( DIFFERENT(ru, RES_USER_UID),    "UID is out of compliance");
		ok( DIFFERENT(ru, RES_USER_GID),    "GID is out of compliance");
		ok( DIFFERENT(ru, RES_USER_GECOS),  "GECOS is out of compliance");
		ok(!DIFFERENT(ru, RES_USER_SHELL),  "SHELL is in compliance");
		ok(!DIFFERENT(ru, RES_USER_DIR),    "DIR is in compliance");
		ok( DIFFERENT(ru, RES_USER_MKHOME), "MKHOME is out of compliance");
		ok( DIFFERENT(ru, RES_USER_PWMIN),  "PWMIN is out of compliance");
		ok( DIFFERENT(ru, RES_USER_PWMAX),  "PWMAX is out of compliance");
		ok( DIFFERENT(ru, RES_USER_PWWARN), "PWWARN is out of compliance");

#if TEST_AS_ROOT
		struct report *report;
		isnt_nul(report = res_user_fixup(ru, 0, &env), "got a fixup report");
		is_int(report->fixed,     1, "user is fixed");
		is_int(report->compliant, 1, "user is compliant");

		is_string(ru->ru_pw->pw_name,  "svc",                 "passwd username");
		is_int(   ru->ru_pw->pw_uid,   7001,                  "passwd UID");
		is_int(   ru->ru_pw->pw_gid,   8001,                  "passwd GID");
		is_string(ru->ru_pw->pw_gecos, "SVC service account", "passwd GECOS field");
		is_string(ru->ru_pw->pw_shell, "/sbin/nologin",       "passwd shell");
		is_string(ru->ru_pw->pw_dir,   "/tmp/nonexistent",    "passwd home dir");
		is_string(ru->ru_sp->sp_namp,  "svc",                 "shadow username");
		is_int(   ru->ru_sp->sp_min,   4,                     "shadow min pw age");
		is_int(   ru->ru_sp->sp_max,   45,                    "shadow max pw age");
		is_int(   ru->ru_sp->sp_warn,  3,                     "shadow pw warning");

		report_free(report);
#endif
		res_user_free(ru);
		pwdb_free(env.user_pwdb);
		spdb_free(env.user_spdb);
	}

	subtest {
		struct res_user *ru;
		struct resource_env env;

		ru = res_user_new("new_user");
		res_user_set(ru, "uid",      "7010");
		res_user_set(ru, "gid",      "20");
		res_user_set(ru, "shell",    "/sbin/nologin");
		res_user_set(ru, "comment",  "New Account");
		res_user_set(ru, "home",     "t/tmp/new_user.home");
		res_user_set(ru, "skeleton", "/etc/skel.svc");

		env.user_pwdb = pwdb_init(PASSWD_DB);
		if (!env.user_pwdb) bail("failed to read passwd database");

		env.user_spdb = spdb_init(SHADOW_DB);
		if (!env.user_spdb) bail("failed to read shadow database");

		ok(res_user_stat(ru, &env) == 0, "res_user_stat succeeds");

#if TEST_AS_ROOT
		struct report *report;
		isn_null(report = res_user_fixup(ru, 0, &env), "fixed up");
		is_int(report->fixed,     1, "user is fixed");
		is_int(report->compliant, 1, "user is compliant");

		is_string(ru->ru_pw->pw_name,  "new_user",            "passwd username");
		is_int(   ru->ru_pw->pw_uid,   7010,                  "passwd UID");
		is_int(   ru->ru_pw->pw_gid,   20,                    "passwd GID");
		is_string(ru->ru_pw->pw_gecos, "New Account",         "passwd GECOS field");
		is_string(ru->ru_pw->pw_shell, "/sbin/nologin",       "passwd shell");
		is_string(ru->ru_pw->pw_dir,   "t/tmp/new_user.home", "passwd home dir");
		is_string(ru->ru_sp->sp_namp,  "new_user",            "shadow username");

		report_free(report);
#endif
		res_user_free(ru);
		pwdb_free(env.user_pwdb);
		spdb_free(env.user_spdb);
	}

	subtest {
		struct res_user *ru;
		struct resource_env env, env_after;
		struct report *report;

		ru = res_user_new("sys");
		res_user_set(ru, "present", "no"); /* Remove the user */

		env.user_pwdb = pwdb_init(PASSWD_DB);
		if (!env.user_pwdb) bail("failed to read passwd database");

		env.user_spdb = spdb_init(SHADOW_DB);
		if (!env.user_spdb) bail("failed to read shadow database");

		ok(res_user_stat(ru, &env) == 0, "res_user_stat succeeds");
		isnt_null(ru->ru_pw, "found user 'sys' in passwd db");
		isnt_null(ru->ru_sp, "found user 'sys' in shadow db");

		isnt_null(report = res_user_fixup(ru, 0, &env), "fixup report");
		is_int(report->fixed,     1, "user is fixed");
		is_int(report->compliant, 1, "user is compliant");

		ok(pwdb_write(env.user_pwdb, NEW_PASSWD_DB) == 0, "saved passwd db");
		ok(spdb_write(env.user_spdb, NEW_SHADOW_DB) == 0, "saved shadow db");

		env_after.user_pwdb = pwdb_init(NEW_PASSWD_DB);
		if (!env_after.user_pwdb) bail("failed to re-read passwd database");

		env_after.user_spdb = spdb_init(NEW_SHADOW_DB);
		if (!env_after.user_spdb) bail("failed to re-read shadow database");

		res_user_free(ru);
		ru = res_user_new("sys");
		ok(res_user_stat(ru, &env_after) == 0, "user stat (after)");
		is_null(ru->ru_pw, "no entry in passwd db, post-fixup");
		is_null(ru->ru_sp, "no entry in shadow db, post-fixup");

		res_user_free(ru);
		pwdb_free(env.user_pwdb);
		spdb_free(env.user_spdb);
		pwdb_free(env_after.user_pwdb);
		spdb_free(env_after.user_spdb);
		report_free(report);
	}

	subtest {
		struct res_user *ru;
		struct resource_env env, env_after;
		struct report *report;

		ru = res_user_new("non_existent_user");
		res_user_set(ru, "present", "no"); /* Remove the user */

		env.user_pwdb = pwdb_init(PASSWD_DB);
		if (!env.user_pwdb) bail("failed to read passwd database");

		env.user_spdb = spdb_init(SHADOW_DB);
		if (!env.user_spdb) bail("failed to read shadow database");

		ok(res_user_stat(ru, &env) == 0, "res_user_stat succeeds");
		is_null(ru->ru_pw, "user not found in passwd db");
		is_null(ru->ru_sp, "user not found in shadow db");

		isnt_null(report = res_user_fixup(ru, 0, &env), "fixed up");
		is_int(report->fixed,     0, "user was already compliant");
		is_int(report->compliant, 1, "user is compliant");

		ok(pwdb_write(env.user_pwdb, NEW_PASSWD_DB) == 0, "saved passwd db");
		ok(spdb_write(env.user_spdb, NEW_SHADOW_DB) == 0, "saved shadow db");

		env_after.user_pwdb = pwdb_init(NEW_PASSWD_DB);
		if (!env_after.user_pwdb) bail("failed to re-read passwd database");

		env_after.user_spdb = spdb_init(NEW_SHADOW_DB);
		if (!env_after.user_spdb) bail("failed to re-read shadow database");

		res_user_free(ru);
		ru = res_user_new("non_existent_user");
		ok(res_user_stat(ru, &env_after) == 0, "res_user_stat succeeds");
		is_null(ru->ru_pw, "no user in passwd db");
		is_null(ru->ru_sp, "no user in shadow db");

		res_user_free(ru);
		pwdb_free(env.user_pwdb);
		spdb_free(env.user_spdb);
		pwdb_free(env_after.user_pwdb);
		spdb_free(env_after.user_spdb);
		report_free(report);
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
		char *packed;
		const char *expected;

		ru = res_user_new("user");
		res_user_set(ru, "uid",      "123"); /* hex: 0000007b */
		res_user_set(ru, "gid",      "999"); /* hex: 000003e7 */
		res_user_set(ru, "password", "sooper.seecret");
		res_user_set(ru, "changepw", "yes");
		res_user_set(ru, "home",     "/home/user");
		res_user_set(ru, "gecos",    "GECOS for user");
		res_user_set(ru, "shell",    "/sbin/nologin");
		res_user_set(ru, "makehome", "/etc/skel.oper");
		res_user_set(ru, "pwmin",    "4");    /* hex: 00000004 */
		res_user_set(ru, "pwmax",    "90");   /* hex: 0000005a */
		res_user_set(ru, "pwwarn",   "14");   /* hex: 0000000e */
		res_user_set(ru, "inact",    "1000"); /* hex: 000003e8 */
		res_user_set(ru, "expiry",   "2000"); /* hex: 000007d0 */
		res_user_set(ru, "locked",   "no");

		packed = res_user_pack(ru);
		expected = "res_user::\"user\""
			"00003fff" /* RES_USER_*, all OR'ed together */
			"\"user\"\"sooper.seecret\""
			"0000007b" /* uid 123 */
			"000003e7" /* gid 999 */
			"\"GECOS for user\"\"/sbin/nologin\"\"/home/user\""
			"01" /* mkhome 1 */
			"\"/etc/skel.oper\""
			"00" /* lock 0 */
			"00000004" /* pwmin 4 */
			"0000005a" /* pwmax 90 */
			"0000000e" /* pwwarn 14 */
			"000003e8" /* inact 1000 */
			"000007d0" /* expire 2000 */
			"";
		is_string(expected, packed, "user packed properly");

		res_user_free(ru);
		free(packed);
	}

	subtest {
		struct res_user *ru;
		char *packed;

		packed = "res_user::\"userkey\""
			"00003001" /* NAME, EXPIRE and LOCK only */
			"\"user\"\"secret\""
			"0000002d" /* uid 45 */
			"0000005a" /* gid 90 */
			"\"GECOS\"\"/bin/bash\"\"/home/user\""
			"01" /* mkhome 1 */
			"\"/etc/skel.oper\""
			"00" /* lock 0 */
			"00000004" /* pwmin 4 */
			"00000032" /* pwmax 50 */
			"00000007" /* pwwarn 7 */
			"00001770" /* inact 6000 */
			"00002328" /* expire 9000 */
			"";

		is_null(res_user_unpack("<invalid packed data>"),
				"res_user_unpack handles bad data");
		isnt_null(ru = res_user_unpack(packed), "res_user_unpack succeeds");
		is_string(ru->key, "userkey", "unpacked user key");

		ok( ENFORCED(ru, RES_USER_NAME),   "NAME is enforced");
		ok( ENFORCED(ru, RES_USER_LOCK),   "LOCK is enforced");
		ok( ENFORCED(ru, RES_USER_EXPIRE), "EXPIRE is enforced");
		ok(!ENFORCED(ru, RES_USER_PASSWD), "PASSWD is not enforced");
		ok(!ENFORCED(ru, RES_USER_UID),    "UID is not enforced");
		ok(!ENFORCED(ru, RES_USER_GID),    "GID is not enforced");
		ok(!ENFORCED(ru, RES_USER_GECOS),  "GECOS is not enforced");
		ok(!ENFORCED(ru, RES_USER_DIR),    "DIR is not enforced");
		ok(!ENFORCED(ru, RES_USER_MKHOME), "MKHOME is not enforced");
		ok(!ENFORCED(ru, RES_USER_SHELL),  "SHELL is not enforced");
		ok(!ENFORCED(ru, RES_USER_PWMIN),  "PWMIN is not enforced");
		ok(!ENFORCED(ru, RES_USER_PWMAX),  "PWMAX is not enforced");
		ok(!ENFORCED(ru, RES_USER_PWWARN), "PWWARN is not enforced");
		ok(!ENFORCED(ru, RES_USER_INACT),  "INACT is not enforced");

		is_string(ru->ru_name,   "user",           "unpacked user username");
		is_string(ru->ru_passwd, "secret",         "unpacked user password");
		is_int(   ru->ru_uid,    45,               "unpacked user UID");
		is_int(   ru->ru_gid,    90,               "unpacked user GID");
		is_string(ru->ru_gecos,  "GECOS",          "unpacked user GECOS");
		is_string(ru->ru_dir,    "/home/user",     "unpacked user home dir");
		is_string(ru->ru_shell,  "/bin/bash",      "unpacked user shell");
		is_int(   ru->ru_mkhome, 1,                "unpacked user mkhome flag");
		is_string(ru->ru_skel,   "/etc/skel.oper", "unpacked user home skeleton dir");
		is_int(   ru->ru_lock,   0,                "unpacked user lock flag");
		is_int(   ru->ru_pwmin,  4,                "unpacked user password min age");
		is_int(   ru->ru_pwmax,  50,               "unpacked user password max age");
		is_int(   ru->ru_pwwarn, 7,                "unpacked user password warning");
		is_int(   ru->ru_inact,  6000,             "unpacked user inactivity deadline");
		// FIXME: why is this failing?
		//is_int(   ru->ru_expire, 9000,             "unpacked user expiry");

		res_user_free(ru);
	}

	subtest {
		struct res_user *ru;
		struct hash *h;

		isnt_null(h = hash_new(), "hash created");
		isnt_null(ru = res_user_new("user"), "res_user created");

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

		res_user_set(ru, "locked", "no");
		ok(res_user_attrs(ru, h) == 0, "got raw user attributes");
		is_string(hash_get(h, "locked"), "no", "h.locked");

		hash_free(h);
		res_user_free(ru);
	}

	subtest { /* cfm-20 - create a user with a conflicting UID */
		struct res_user *ru;
		struct report *report;
		struct resource_env env;
		struct resource_env env_after;

		ru = res_user_new("conflictd");
		res_user_set(ru, "present", "yes");
		res_user_set(ru, "uid", "3"); /* conflict with sys user */

		env.user_pwdb = pwdb_init(PASSWD_DB);
		if (!env.user_pwdb) bail("failed to read passwd db");

		env.user_spdb = sgdb_init(SHADOW_DB);
		if (!env.user_spdb) bail("failed to read gshadow db");

		ok(res_user_stat(ru, &env) == 0, "res_user_stat succeeds");

		isnt_null(report = res_user_fixup(ru, 0, &env), "user fixup");

		is_int(report->fixed,     0, "user is not fixed");
		is_int(report->compliant, 0, "user is non-compliant");

		ok(pwdb_write(env.user_pwdb, NEW_PASSWD_DB)   == 0, "saved passwd db");
		ok(sgdb_write(env.user_spdb, NEW_SHADOW_DB) == 0, "saved shadow db");

		env_after.user_pwdb = pwdb_init(NEW_PASSWD_DB);
		if (!env_after.user_pwdb) bail("failed to re-read passwd db");

		env_after.user_spdb = sgdb_init(NEW_SHADOW_DB);
		if (!env_after.user_spdb) bail("failed to re-read shadow db");

		is_null(pwdb_get_by_name(env_after.user_pwdb, "conflictd"),
			"user conflictd should not have been created in pwdb");
		is_null(sgdb_get_by_name(env_after.user_spdb, "conflictd"),
			"user conflictd should not have been created in spdb");
	}

	subtest { /* cfm-20 - edit an existing user with a conflicting UID */
		struct res_user *ru;
		struct report *report;
		struct resource_env env;
		struct resource_env env_after;

		ru = res_user_new("daemon");
		res_user_set(ru, "present", "yes");
		res_user_set(ru, "uid", "3"); /* conflict with sys user */

		env.user_pwdb = grdb_init(PASSWD_DB);
		if (!env.user_pwdb) bail("failed to read passwd db");

		env.user_spdb = sgdb_init(SHADOW_DB);
		if (!env.user_spdb) bail("failed to read gshadow db");

		ok(res_user_stat(ru, &env) == 0, "res_user_stat succeeds");

		isnt_null(report = res_user_fixup(ru, 0, &env), "user fixup");

		is_int(report->fixed,     0, "user is not fixed");
		is_int(report->compliant, 0, "user is non-compliant");

		ok(grdb_write(env.user_pwdb, NEW_PASSWD_DB)   == 0, "saved passwd db");
		ok(sgdb_write(env.user_spdb, NEW_SHADOW_DB) == 0, "saved gshadow db");

		env_after.user_pwdb = grdb_init(NEW_PASSWD_DB);
		if (!env_after.user_pwdb) bail("failed to re-read passwd db");

		env_after.user_spdb = sgdb_init(NEW_SHADOW_DB);
		if (!env_after.user_spdb) bail("failed to re-read gshadow db");

		struct passwd *pw = grdb_get_by_name(env_after.user_pwdb, "daemon");
		isnt_null(pw, "daemon user should still exist in passwd db");
		is_int(pw->pw_uid, 1, "daemon user UID unchanged");
	}

	done_testing();
}
