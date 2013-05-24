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

		res_file_set(rf, "path", "t/tmp/file");
		is_string(rf->rf_lpath, "t/tmp/file", "target file to enforce");

		res_file_free(rf);
	}

	subtest {
		struct res_file *rf;

		rf = res_file_new("sudoers");
		res_file_set(rf, "path", "t/data/res_file/sudoers");
		res_file_set(rf, "owner", "someuser");
		rf->rf_uid = 1001;
		res_file_set(rf, "group", "somegroup");
		rf->rf_gid = 2002;
		res_file_set(rf, "mode", "0440");

		/* don't need to pass in an env arg (2nd param) */
		ok(res_file_stat(rf, NULL) == 0, "res_file_stat is ok");
		is_int(rf->rf_exists, 1, "file exists");
		ok(DIFFERENT(rf, RES_FILE_UID), "UID is out of compliance");
		ok(DIFFERENT(rf, RES_FILE_GID), "GID is out of compliance");
		ok(DIFFERENT(rf, RES_FILE_MODE), "MODE is out of compliance");

		res_file_free(rf);
	}

	subtest {
#if TEST_AS_ROOT
		struct stat st;
		struct res_file *rf;
		struct resource_env env;
		struct report *report;

		sys("mkdir -p t/tmp/res_file");
		sys("cp -a t/data/res_file/fstab t/tmp/res_file/fstab");

		/* STAT the target file */
		if (stat("t/tmp/res_file/fstab", &st) != 0)
			bail("failed to stat pre-remediation file");

		isnt_int(st.st_uid, 65542, "file owner pre-fixup");
		isnt_int(st.st_gid, 65524, "file group pre-fixup");
		isnt_int(st.st_mode & 0777, 0754, "file mode pre-fixup");

		rf = res_file_new("fstab");
		res_file_set(rf, "path",  "t/tmp/res_file/fstab");
		res_file_set(rf, "owner", "someuser");
		rf->rf_uid = 65542;
		res_file_set(rf, "group", "somegroup");
		rf->rf_gid = 65524;
		res_file_set(rf, "mode",  "0754");
		res_file_set(rf, "source", "t/data/res_file/fstab");

		/* set up the resource_env for content remediation */
		env.file_fd = open("t/data/res_file/fstab", O_RDONLY);
		ok(env.file_fd > 0, "opened 'remote' file");
		env.file_len = st.st_size;

		ok(res_file_stat(rf, &env) == 0, "res_file_stat succeeds");
		is_int(rf->rf_exists, 1, "file exists");
		ok(DIFFERENT(rf, RES_FILE_UID), "UID is out of compliance");
		ok(DIFFERENT(rf, RES_FILE_GID), "GID is out of compliance");

		isnt_null(report = res_file_fixup(rf, 0, &env), "file fixup");
		is_int(report->fixed,     1, "file is fixed");
		is_int(report->compliant, 1, "file is compliant");

		/* STAT the fixed up file */
		if (stat("t/tmp/res_file/fstab", &st) != 0)
			vail("failed to stat post-remediation file");
		is_int(st.st_uid, 65542, "file owner post-fixup");
		is_int(st.st_gid, 65524, "file group post-fixup");
		is_int(st.st_mode & 07777, 0754, "file mode post-fixup");

		res_file_free(rf);
		report_free(report);
#endif
	}

	subtest {
#if TEST_AS_ROOT
		struct stat st;
		struct res_file *rf;
		struct report *report;
		struct resource_env env;

		sys("rm -f t/tmp/res_file/new_file");

		env.file_fd = -1;
		env.file_len = 0;

		rf = res_file_new("t/tmp/res_file/new_file");
		res_file_set(rf, "owner", "someuser");
		rf->rf_uid = 65542;
		res_file_set(rf, "group", "somegroup");
		rf->rf_gid = 65524;
		res_file_set(rf, "mode",  "0754");

		ok(res_file_stat(rf, &env) == 0, "res_file_stat succeeds");
		is_int(rf->rf_exists, 0, "file does not already exists");
		ok(stat("t/tmp/res_file/new_file", &st) != 0, "pre-fixup stat fails");

		isnt_null(report = res_file_fixup(rf, 0, &env), "file fixup");
		is_int(report->fixed,     1, "file is fixed");
		is_int(report->compliant, 1, "file is compliant");
		ok(stat("t/tmp/res_file/new_file", &st) == 0, "post-fixup stat succeeds");

		is_int(st.st_uid, 65542, "file UID");
		is_int(st.st_gid, 65524, "file GID");
		is_int(st.st_mode & 07777, 0754, "file mode");

		res_file_free(rf);
		report_free(report);
#endif
	}

	subtest {
#if TEST_AS_ROOT
		struct stat st;
		struct res_file *rf;
		struct report *report;
		struct resource_env env;

		sys("cp -a t/data/res_file/delete t/tmp/res_file/delete");
		const char *path = TEST_UNIT_TEMP "/res_file/delete";

		env.file_fd = -1;
		env.file_len = 0;

		rf = res_file_new("delete");
		res_file_set(rf, "present", "no"); /* Remove the file */

		ok(res_file_stat(rf, &env) == 0, "res_file_stat succeeds");
		is_int(rf->rf_exists, 1, "file exists");
		ok(stat("t/tmp/res_file/delete", &st) == 0, "pre-remediation stat succeeds");

		isnt_null(report = res_file_fixup(rf, 0, &env), "file fixup");
		is_int(report->fixed,     1, "file is fixed");
		is_int(report->compliant, 1, "file is compliant");
		ok(stat("t/tmp/res_file/delete", &st) != 0, "pre-remediation stat fails");

		res_file_free(rf);
		report_free(report);
#endif
	}

	subtest {
		struct stat st;
		struct res_file *rf;
		struct report *report;
		struct resource_env env;

		sys("rm -f t/tmp/res_file/enoent");

		env.file_fd = -1;
		env.file_len = 0;

		rf = res_file_new("enoent");
		res_file_set(rf, "present", "no"); /* Remove the file */

		ok(res_file_stat(rf, &env) == 0, "res_file_stat succeeds");
		is_int(rf->rf_exists, 0, "file doesnt exist");
		ok(stat("t/tmp/res_file/enoent", &st) != 0, "pre-fixup stat fails");

		isnt_null(report = res_file_fixup(rf, 0, &env), "file fixup");
		is_int(report->fixed,     0, "file was already compliant");
		is_int(report->compliant, 1, "file is compliant");
		ok(stat("t/tmp/res_file/enoent", &st) != 0, "post-fixup stat fails");

		res_file_free(rf);
		report_free(report);
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
		char *packed;
		const char *expected;

		rf = res_file_new("/etc/sudoers");                      /* rf_enf == 0000 0000 */

		res_file_set(rf, "owner",  "someuser");             /* rf_enf == 0000 0001 */
		res_file_set(rf, "group",  "somegroup");            /* rf_enf == 0000 0011 */
		res_file_set(rf, "mode",   "0644");                 /* rf_enf == 0000 0111 */
		res_file_set(rf, "source", "/etc/issue");           /* rf_enf == 0000 1111 */
		/* sneakily override the checksum */
		sha1_init(&rf->rf_rsha1, "0123456789abcdef0123456789abcdef01234567");

		packed = res_file_pack(rf);
		expected = "res_file::\"/etc/sudoers\""
			"0000000f" /* RES_FILE_*, all OR'ed together */
			"\"/etc/sudoers\""
			"\"0123456789abcdef0123456789abcdef01234567\""
			"\"someuser\"" /* rf_owner */
			"\"somegroup\"" /* rf_group */
			"000001a4" /* rf_mode 0644 */
			"";
		is_string(expected, packed, "packs properly");

		res_file_free(rf);
		free(packed);
	}

	subtest {
		struct res_file *rf;
		char *packed;

		packed = "res_file::\"somefile\""
			"00000003" /* UID and GID only */
			"\"/etc/sudoers\""
			"\"0123456789abcdef0123456789abcdef01234567\""
			"\"someuser\""  /* rf_owner */
			"\"somegroup\"" /* rf_group */
			"000001a4" /* rf_mode 0644 */
			"";

		is_null(res_file_unpack("<invalid packed data>"),
				"res_file_unpack handles bad data");
		isnt_null(rf = res_file_unpack(packed), "res_file_unpack succeeds");

		is_string(rf->key,      "somefile",     "unpacked file key");
		is_string(rf->rf_lpath, "/etc/sudoers", "unpacked file local path");
		is_string(rf->rf_owner, "someuser",     "unpacked file owner");
		is_string(rf->rf_group, "somegroup",    "unpacked file group");
		is_int(   rf->rf_mode,  0644,           "unpacked file mode");
		is_string(rf->rf_rsha1.hex, "0123456789abcdef0123456789abcdef01234567",
				"unpacked remote SHA1");

		ok(!ENFORCED(rf, RES_FILE_SHA1), "SHA1 is not enforced");
		ok(!ENFORCED(rf, RES_FILE_MODE), "MODE is not enforced");
		ok( ENFORCED(rf, RES_FILE_UID), "UID is not enforced");
		ok( ENFORCED(rf, RES_FILE_GID), "GID is not enforced");

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

		hash_free(h);
		res_file_free(rf);
	}

	done_testing();
}
