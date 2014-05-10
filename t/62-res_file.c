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
		struct res_file *file;
		char *key;

		isnt_null(file = res_file_new("file-key"), "generated file");
		isnt_null(key = res_file_key(file), "got file key");
		is_string(key, "file:file-key", "file key");
		free(key);
		res_file_free(file);
	}

	subtest {
		ok(res_file_notify(NULL, NULL) == 0, "res_file_notify is a NOOP");
	}

	subtest {
		res_file_free(NULL);
		pass("res_file_free(NULL) doesn't segfault");
	}

	subtest {
		struct res_file *rf;

		isnt_null(rf = res_file_new("sudoers"), "created a file resource");
		ok(!ENFORCED(rf, RES_FILE_UID),  "UID not enforced by default");
		ok(!ENFORCED(rf, RES_FILE_GID),  "GID not enforced by default");
		ok(!ENFORCED(rf, RES_FILE_MODE), "MODE not enforced by default");
		ok(!ENFORCED(rf, RES_FILE_SHA1), "SHA1 not enforced by default");

		res_file_set(rf, "owner", "someone");
		ok(ENFORCED(rf, RES_FILE_UID), "UID enforced");
		is_string(rf->rf_owner, "someone", "owner to enforce");

		res_file_set(rf, "group", "somegroup");
		ok(ENFORCED(rf, RES_FILE_GID), "GID enforced");
		is_string(rf->rf_group, "somegroup", "group to enforce");

		res_file_set(rf,"mode", "0755");
		ok(ENFORCED(rf, RES_FILE_MODE), "MODE enforced");
		is_int(rf->rf_mode, 0755, "mode to enforce");

		res_file_set(rf, "source", "t/data/sha1/file");
		ok(ENFORCED(rf, RES_FILE_SHA1), "SHA1 enforced");
		is_string(rf->rf_rpath, "t/data/sha1/file", "source file to enforce");

		res_file_set(rf, "path", TEST_TMP "/file");
		is_string(rf->rf_lpath, TEST_TMP "/file", "target file to enforce");

		res_file_free(rf);
	}

	subtest {
		struct res_file *rf;

		rf = res_file_new("SUDO");
		res_file_set(rf, "owner",  "someuser");
		res_file_set(rf, "path",   "/etc/sudoers");
		res_file_set(rf, "source", "/etc/issue");

		ok(res_file_match(rf, "path", "/etc/sudoers") == 0, "match path=/etc/sudoers");
		ok(res_file_match(rf, "path", "/tmp/wrong")   != 0, "!match path=/tmp/wrong");
		ok(res_file_match(rf, "owner", "someuser")    != 0, "'owner' is not a match attr");

		res_file_free(rf);
	}

	subtest {
		struct res_file *rf;
		struct hash *h;

		isnt_null(h = hash_new(), "created hash");
		isnt_null(rf = res_file_new("/etc/sudoers"), "created file resource");

		ok(res_file_attrs(rf, h) == 0, "got raw file attrs");
		is_string(hash_get(h, "path"),    "/etc/sudoers", "h.path");
		is_string(hash_get(h, "present"), "yes",          "h.present"); // default
		is_null(hash_get(h, "owner"),    "h.owner is unset");
		is_null(hash_get(h, "group"),    "h.group is unset");
		is_null(hash_get(h, "mode"),     "h.mode is unset");
		is_null(hash_get(h, "template"), "h.template is unset");
		is_null(hash_get(h, "source"),   "h.source is unset");

		res_file_set(rf, "owner",  "root");
		res_file_set(rf, "group",  "sys");
		res_file_set(rf, "mode",   "0644");
		res_file_set(rf, "source", "/srv/files/sudo");

		ok(res_file_attrs(rf, h) == 0, "got raw file attrs");
		is_string(hash_get(h, "owner"),  "root",            "h.owner");
		is_string(hash_get(h, "group"),  "sys",             "h.group");
		is_string(hash_get(h, "mode"),   "0644",            "h.mode");
		is_string(hash_get(h, "source"), "/srv/files/sudo", "h.source");
		is_null(hash_get(h, "template"), "h.template is unset");

		res_file_set(rf, "present", "no");
		res_file_set(rf, "template", "/srv/tpl/sudo");

		ok(res_file_attrs(rf, h) == 0, "got raw file attrs");
		is_string(hash_get(h, "template"), "/srv/tpl/sudo", "h.template");
		is_string(hash_get(h, "present"),  "no",            "h.present");
		is_null(hash_get(h, "source"), "h.source is unset");

		ok(res_file_set(rf, "xyzzy", "bad") != 0, "xyzzy is not a valid attr");
		ok(res_file_attrs(rf, h) == 0, "got raw file attrs");
		is_null(hash_get(h, "xyzzy"), "h.xyzzy is unset");

		hash_free_all(h);
		res_file_free(rf);
	}

	done_testing();
}
