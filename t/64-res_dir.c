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

int main(void) {
	test();

	subtest {
		struct res_dir *dir;
		char *key;

		dir = res_dir_new("/path/to/dir");
		key = res_dir_key(dir);
		is_string(key, "dir:/path/to/dir", "dir key formatted properly");
		free(key);
		res_dir_free(dir);
	}

	subtest {
		ok(res_dir_notify(NULL, NULL) == 0, "res_dir_notify is a NOOP");
	}

	subtest {
		res_dir_free(NULL);
		pass("res_dir_free(NULL) doesn't segfault");
	}

	subtest {
		struct res_dir *r;
		r = res_dir_new("/tmp");

		ok(!ENFORCED(r, RES_DIR_ABSENT), "ABSENT is not enforced");
		ok(!ENFORCED(r, RES_DIR_UID),    "UID is not enforced");
		ok(!ENFORCED(r, RES_DIR_GID),    "GID is not enforced");
		ok(!ENFORCED(r, RES_DIR_MODE),   "MODE is not enforced");

		res_dir_set(r, "present", "no");
		ok(ENFORCED(r, RES_DIR_ABSENT), "ABSENT is enforced");

		res_dir_set(r, "present", "yes");
		ok(!ENFORCED(r, RES_DIR_ABSENT), "ABSENT is not enforced");

		res_dir_set(r, "owner", "user1");
		ok(ENFORCED(r, RES_DIR_UID), "UID is enforced");
		is_string(r->owner, "user1", "owner to enforce");

		res_dir_set(r, "group", "staff");
		ok(ENFORCED(r, RES_DIR_GID), "GID is enforced");
		is_string(r->group, "staff", "group to enforce");

		res_dir_set(r, "mode", "0750");
		ok(ENFORCED(r, RES_DIR_MODE), "MODE is enforced");
		is_int(r->mode, 0750, "dir mode to enforce");

		res_dir_free(r);
	}

	subtest {
		struct res_dir *r;

		r = res_dir_new("dir1");
		res_dir_set(r, "path", "t/data");
		res_dir_set(r, "owner", "someuser");
		r->uid = 1001;
		res_dir_set(r, "group", "staff");
		r->gid = 2002;
		res_dir_set(r, "mode", "0705");

		/* don't need to pass in an env arg (2nd param) */
		ok(res_dir_stat(r, NULL) == 0, "res_dir_stat succeeds");
		is_int(r->exists, 1, "Direcotry exists");

		ok(DIFFERENT(r, RES_DIR_UID),  "UID is out of compliance");
		ok(DIFFERENT(r, RES_DIR_GID),  "GID is out of compliance");
		ok(DIFFERENT(r, RES_DIR_MODE), "MODE is out of compliance");

		res_dir_free(r);
	}

	subtest {
#if TEST_AS_ROOT
		struct stat st;
		struct res_dir *r;
		struct resource_env env; // needed but not used
		struct report *report;

		const char *path = TEST_UNIT_TEMP "/res_dir/fixme";

		sys("mkdir -p t/tmp/fixme-dir");
		if (stat("t/tmp/fixme-dir", &st) != 0)
			bail("Failed to stat pre-fixup dir");
		isnt_int(st.st_uid, 65542, "pre-fixup directory owner");
		isnt_int(st.st_gid, 65524, "pre-fixup directory group");
		isnt_int(st.st_mode & 07777, 0754, "pre-fixup directory mode");

		r = res_dir_new("fix");
		res_dir_set(r, "path",  "t/tmp/fixme-dir");
		res_dir_set(r, "owner", "someuser");
		r->uid = 65542;
		res_dir_set(r, "group", "somegroup");
		r->gid = 65524;
		res_dir_set(r, "mode", "0754");

		ok(res_dir_stat(r, &env) == 0, "res_dir_stat succeeds");
		is_int(r->exists, 1, "Directory exists");
		ok(DIFFERENT(r, RES_DIR_UID), "UID is out of compliance");
		ok(DIFFERENT(r, RES_DIR_GID), "GID is out of compliance");

		isnt_null(report = res_dir_fixup(r, 0, &env), "dir fixup");
		is_int(report->fixed,     1, "dir is fixed");
		is_int(report->compliant, 1, "dir is compliant");

		if (stat("t/tmp/fixme-dir", &st) != 0)
			bail("Failed to stat post-fixup dir");
		is_int(st.st_uid, 65542, "post-fixup directory owner");
		is_int(st.st_gid, 65524, "post-fixup directory group");
		is_int(st.st_mode & 07777, 0754, "post-fixup directory mode");

		res_dir_free(r);
		report_free(report);
#endif
	}

	subtest {
		struct res_dir *rd;

		rd = res_dir_new("tmpdir");
		res_dir_set(rd, "path",  "/tmp");
		res_dir_set(rd, "owner", "root");
		res_dir_set(rd, "group", "sys");
		res_dir_set(rd, "mode",  "1777");

		ok(res_dir_match(rd, "path", "/tmp") == 0, "match path=/tmp");
		ok(res_dir_match(rd, "path", "/usr") != 0, "!match path=/usr");

		ok(res_dir_match(rd, "owner", "root") != 0, "owner is not a matchable attr");
		ok(res_dir_match(rd, "group", "sys")  != 0, "group is not a matchable attr");
		ok(res_dir_match(rd, "mode",  "1777") != 0, "mode is not a matchable attr");

		res_dir_free(rd);
	}

	subtest {
		struct res_dir *rd;
		char *packed;
		const char *expected;

		rd = res_dir_new("home");
		res_dir_set(rd, "path",    "/home");
		res_dir_set(rd, "owner",   "root");
		res_dir_set(rd, "group",   "users");
		res_dir_set(rd, "mode",    "0750");
		res_dir_set(rd, "present", "no");

		packed = res_dir_pack(rd);
		/* mode 0750 (octal) = 000001e8 (hex) */
		expected = "res_dir::\"home\"80000007\"/home\"\"root\"\"users\"000001e8";
		is_string(packed, expected, "dir packs properly");

		res_dir_free(rd);
		free(packed);
	}

	subtest {
		struct res_dir *rd;
		const char *packed;

		packed = "res_dir::\"dirkey\""
			"80000007"
			"\"/tmp\""
			"\"root\""
			"\"sys\""
			"000003ff";

		is_null(res_dir_unpack("<invalid packed data>"), "res_dir_unpack handles bad data");
		isnt_null(rd = res_dir_unpack(packed), "res_dir_unpack succeeds");

		ok(ENFORCED(rd, RES_DIR_UID),    "UID is enforced");
		ok(ENFORCED(rd, RES_DIR_GID),    "GID is enforced");
		ok(ENFORCED(rd, RES_DIR_MODE),   "MODE is enforced");
		ok(ENFORCED(rd, RES_DIR_ABSENT), "ABSENT is enforced");

		is_string(rd->key,   "dirkey", "unpacked dir key");
		is_string(rd->path,  "/tmp",   "unpacked dir path");
		is_string(rd->owner, "root",   "unpacked dir owner");
		is_string(rd->group, "sys",    "unpacked dir group");
		is_int(   rd->mode,  01777,    "unpacked dir mode");

		res_dir_free(rd);
	}

	subtest {
		struct res_dir *rd;
		struct hash *h;

		isnt_null(h = hash_new(), "hash created");
		isnt_null(rd = res_dir_new("/tmp"), "res_dir created");

		ok(res_dir_attrs(rd, h) == 0, "restrieved res_dir attrs");
		is_string(hash_get(h, "path"),    "/tmp", "h.path");
		is_string(hash_get(h, "present"), "yes",  "h.present");
		is_null(hash_get(h, "owner"), "h.owner is NULL");
		is_null(hash_get(h, "group"), "h.group is NULL");
		is_null(hash_get(h, "mode"),  "h.mode NULL");

		res_dir_set(rd, "owner",   "root");
		res_dir_set(rd, "group",   "sys");
		res_dir_set(rd, "mode",    "01777");
		res_dir_set(rd, "present", "yes");

		ok(res_dir_attrs(rd, h) == 0, "restrieved res_dir attrs");
		is_string(hash_get(h, "path"),    "/tmp", "h.path");
		is_string(hash_get(h, "owner"),   "root", "h.owner");
		is_string(hash_get(h, "group"),   "sys",  "h.group");
		is_string(hash_get(h, "mode"),    "1777", "h.mode");
		is_string(hash_get(h, "present"), "yes",  "h.present");

		res_dir_set(rd, "present", "no");
		ok(res_dir_attrs(rd, h) == 0, "restrieved res_dir attrs");
		is_string(hash_get(h, "present"), "no",   "h.present");

		ok(res_dir_set(rd, "xyzzy", "BAD") != 0, "xyzzy is not a valid attribute");
		ok(res_dir_attrs(rd, h) == 0, "restrieved res_dir attrs");
		is_null(hash_get(h, "xyzzy"), "h.xyzzy is unset (bad attr)");

		hash_free(h);
		res_dir_free(rd);
	}

	done_testing();
}
