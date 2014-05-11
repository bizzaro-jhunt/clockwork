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

TESTS {
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

		is_int(res_dir_set(r, "what-does-the-fox-say", "ring-ding-ring-ding"),
			-1, "res_dir_set doesn't like nonsensical attributes");

		res_dir_free(r);
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
		cw_hash_t *h;

		isnt_null(h = cw_alloc(sizeof(cw_hash_t)), "hash created");
		isnt_null(rd = res_dir_new("/tmp"), "res_dir created");

		ok(res_dir_attrs(rd, h) == 0, "restrieved res_dir attrs");
		is_string(cw_hash_get(h, "path"),    "/tmp", "h.path");
		is_string(cw_hash_get(h, "present"), "yes",  "h.present");
		is_null(cw_hash_get(h, "owner"), "h.owner is NULL");
		is_null(cw_hash_get(h, "group"), "h.group is NULL");
		is_null(cw_hash_get(h, "mode"),  "h.mode NULL");

		res_dir_set(rd, "owner",   "root");
		res_dir_set(rd, "group",   "sys");
		res_dir_set(rd, "mode",    "01777");
		res_dir_set(rd, "present", "yes");

		ok(res_dir_attrs(rd, h) == 0, "restrieved res_dir attrs");
		is_string(cw_hash_get(h, "path"),    "/tmp", "h.path");
		is_string(cw_hash_get(h, "owner"),   "root", "h.owner");
		is_string(cw_hash_get(h, "group"),   "sys",  "h.group");
		is_string(cw_hash_get(h, "mode"),    "1777", "h.mode");
		is_string(cw_hash_get(h, "present"), "yes",  "h.present");

		res_dir_set(rd, "present", "no");
		ok(res_dir_attrs(rd, h) == 0, "restrieved res_dir attrs");
		is_string(cw_hash_get(h, "present"), "no",   "h.present");

		ok(res_dir_set(rd, "xyzzy", "BAD") != 0, "xyzzy is not a valid attribute");
		ok(res_dir_attrs(rd, h) == 0, "restrieved res_dir attrs");
		is_null(cw_hash_get(h, "xyzzy"), "h.xyzzy is unset (bad attr)");

		cw_hash_done(h, 1);
		res_dir_free(rd);
	}

	done_testing();
}
