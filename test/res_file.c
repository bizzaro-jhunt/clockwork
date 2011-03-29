#include "test.h"
#include "assertions.h"
#include "sha1_files.h"
#include "../res_file.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


void test_res_file_enforcement()
{
	struct res_file *rf;
	rf = res_file_new("sudoers");
	const char *src = "test/data/sha1/file1";

	test("RES_FILE: Default Enforcements");
	assert_true("UID not enforced",  !res_file_enforced(rf, UID));
	assert_true("GID not enforced",  !res_file_enforced(rf, GID));
	assert_true("MODE not enforced", !res_file_enforced(rf, MODE));
	assert_true("SHA1 not enforced", !res_file_enforced(rf, SHA1));

	test("RES_FILE: UID enforcement");
	res_file_set(rf, "owner", "someone");
	assert_true("owner enforced", res_file_enforced(rf, UID));
	assert_str_equals("owner set properly", rf->rf_owner, "someone");

	test("RES_FILE: GID enforcement");
	res_file_set(rf, "group", "somegroup");
	assert_true("group enforced", res_file_enforced(rf, GID));
	assert_str_equals("group set properly", rf->rf_group, "somegroup");

	test("RES_FILE: MODE enforcement");
	res_file_set(rf,"mode", "0755");
	assert_true("mode enforced", res_file_enforced(rf, MODE));
	assert_int_equals("mode set properly", rf->rf_mode, 0755);

	test("RES_FILE: SHA1 / rpath enforcement");
	res_file_set(rf, "source", src);
	assert_true("SHA1 enforced", res_file_enforced(rf, SHA1));
	assert_str_equals("SHA1 set properly", rf->rf_rsha1.hex, TESTFILE_SHA1_file1);
	assert_str_equals("rf_rpath set properly", rf->rf_rpath, src);

	test("RES_FILE: SHA1 / lpath 'enforcement'");
	res_file_set(rf, "path", src);
	assert_str_equals("rf_lpath set properly", rf->rf_lpath, src);

	res_file_free(rf);
}

void test_res_file_diffstat()
{
	struct res_file *rf;

	rf = res_file_new("sudoers");
	res_file_set(rf, "path", "test/data/res_file/sudoers");
	res_file_set(rf, "owner", "someuser");
	rf->rf_uid = 1001;
	res_file_set(rf, "group", "somegroup");
	rf->rf_gid = 2002;
	res_file_set(rf, "mode", "0440");

	test("RES_FILE: res_file_diffstat picks up file differences");
	/* 2nd arg to res_file_stat is NULL, because we don't need it in res_file */
	assert_int_equals("res_file_stat returns zero", res_file_stat(rf, NULL), 0);
	assert_int_equals("File exists", 1, rf->rf_exists);
	assert_true("UID is out of compliance",  res_file_different(rf, UID));
	assert_true("GID is out of compliance",  res_file_different(rf, GID));
	assert_true("MODE is out of compliance", res_file_different(rf, MODE));

	res_file_free(rf);
}

void test_res_file_remedy()
{
	struct stat st;
	struct res_file *rf;
	struct resource_env env;

	struct report *report;

	const char *path = "test/data/res_file/fstab";
	const char *src  = "test/data/res_file/SRC/fstab";

	test("RES_FILE: File Remediation");

	/* STAT the target file */
	if (stat(path, &st) != 0) {
		assert_fail("RES_FILE: Unable to stat pre-remediation file");
		return;
	}
	assert_int_not_equal("Pre-remediation: file owner UID is not 65542", st.st_uid, 65542);
	assert_int_not_equal("Pre-remediation: file group GID is not 65524", st.st_gid, 65524);
	assert_int_not_equal("Pre-remediation: file permissions are not 0754", st.st_mode & 07777, 0754);

	rf = res_file_new("fstab");
	res_file_set(rf, "path",  "test/data/res_file/fstab");
	res_file_set(rf, "owner", "someuser");
	rf->rf_uid = 65542;
	res_file_set(rf, "group", "somegroup");
	rf->rf_gid = 65524;
	res_file_set(rf, "mode",  "0754");
	res_file_set(rf, "source", src);

	/* set up the resource_env for content remediation */
	env.file_fd = open(src, O_RDONLY);
	assert_int_greater_than("(test sanity) able to open 'remote' file", env.file_fd, 0);
	env.file_len = st.st_size;

	assert_int_equals("res_file_stat succeeds", res_file_stat(rf, &env), 0);
	assert_int_equals("File exists", 1, rf->rf_exists);
	assert_true("UID is out of compliance",  res_file_different(rf, UID));
	assert_true("GID is out of compliance",  res_file_different(rf, GID));
	report = res_file_fixup(rf, 0, &env);
	assert_not_null("res_file_fixup returns a report", report);
	assert_int_equals("file was fixed", report->fixed, 1);
	assert_int_equals("file is now compliant", report->compliant, 1);

	/* STAT the fixed up file */
	if (stat(path, &st) != 0) {
		assert_fail("RES_FILE: Unable to stat post-remediation file");
		return;
	}
	assert_int_equals("Post-remediation: file owner UID 65542", st.st_uid, 65542);
	assert_int_equals("Post-remediation: file group GID 65524", st.st_gid, 65524);
	assert_int_equals("Post-remediation: file permissions are 0754", st.st_mode & 07777, 0754);

	res_file_free(rf);
	report_free(report);
}

void test_res_file_fixup_new()
{
	struct stat st;
	struct res_file *rf;
	struct report *report;
	struct resource_env env;

	const char *path = "test/data/res_file/new_file";

	env.file_fd = -1;
	env.file_len = 0;

	test("RES_FILE: File Remediation (new file)");
	rf = res_file_new(path);
	res_file_set(rf, "owner", "someuser");
	rf->rf_uid = 65542;
	res_file_set(rf, "group", "somegroup");
	rf->rf_gid = 65524;
	res_file_set(rf, "mode",  "0754");

	assert_int_equals("res_file_stat succeeds", res_file_stat(rf, &env), 0);
	assert_int_equals("File does not already exist", 0, rf->rf_exists);
	assert_int_not_equal("stat pre-remediation file returns non-zero", 0, stat(path, &st));

	report = res_file_fixup(rf, 0, &env);
	assert_not_null("res_file_fixup returns a report", report);
	assert_int_equals("file is fixed", report->fixed, 1);
	assert_int_equals("file is now compliant", report->compliant, 1);
	assert_int_equals("stat post-remediation file returns zero", 0, stat(path, &st));

	/* STAT the fixed up file */
	if (stat(path, &st) != 0) {
		assert_fail("RES_FILE: Unable to stat post-remediation file");
		return;
	}
	assert_int_equals("Post-remediation: file owner UID 65542", st.st_uid, 65542);
	assert_int_equals("Post-remediation: file group GID 65524", st.st_gid, 65524);
	assert_int_equals("Post-remediation: file permissions are 0754", st.st_mode & 07777, 0754);

	res_file_free(rf);
	report_free(report);
}

void test_res_file_fixup_remove_existing()
{
	struct stat st;
	struct res_file *rf;
	struct report *report;
	struct resource_env env;

	const char *path = "test/data/res_file/delete";

	env.file_fd = -1;
	env.file_len = 0;

	test("RES_FILE: File Remediation (remove existing file)");
	rf = res_file_new(path);
	res_file_set(rf, "present", "no"); /* Remove the file */

	assert_int_equals("res_file_stat succeeds", res_file_stat(rf, &env), 0);
	assert_int_equals("File exists", 1, rf->rf_exists);
	assert_int_equals("stat pre-remediation file returns zero", 0, stat(path, &st));

	report = res_file_fixup(rf, 0, &env);
	assert_not_null("res_file_fixup returns a report", report);
	assert_int_equals("file is fixed", report->fixed, 1);
	assert_int_equals("file is now compliant", report->compliant, 1);
	assert_int_not_equal("stat post-remediation file returns non-zero", 0, stat(path, &st));

	res_file_free(rf);
	report_free(report);
}

void test_res_file_fixup_remove_nonexistent()
{
	struct stat st;
	struct res_file *rf;
	struct report *report;
	struct resource_env env;

	const char *path = "test/data/res_file/non-existent";

	env.file_fd = -1;
	env.file_len = 0;

	test("RES_FILE: File Remediation (remove non-existent file)");
	rf = res_file_new(path);
	res_file_set(rf, "present", "no"); /* Remove the file */

	assert_int_equals("res_file_stat succeeds", res_file_stat(rf, &env), 0);
	assert_int_equals("File does not already exist", 0, rf->rf_exists);
	assert_int_not_equal("stat pre-remediation file returns non-zero", 0, stat(path, &st));

	report = res_file_fixup(rf, 0, &env);
	assert_not_null("res_file_fixup returns a report", report);
	assert_int_equals("file was already compliant", report->fixed, 0);
	assert_int_equals("file is now compliant", report->compliant, 1);
	assert_int_not_equal("stat post-remediation file returns non-zero", 0, stat(path, &st));

	res_file_free(rf);
	report_free(report);
}

void test_res_file_pack_detection()
{
	test("RES_FILE: pack detection based on tag");
	assert_int_equals("With leading tag", 0, res_file_is_pack("res_file::"));
	assert_int_not_equal("Bad tag (space issue)", 0, res_file_is_pack("res_file ::"));
	assert_int_not_equal("Bad tag (res_user)", 0, res_file_is_pack("res_user::"));
	assert_int_not_equal("Bad tag (random string)", 0, res_file_is_pack("this is an ARBITRARY string"));
}

void test_res_file_pack()
{
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

	test("RES_FILE: file serialization");
	packed = res_file_pack(rf);
	expected = "res_file::"
		"0000000f" /* RES_FILE_*, all OR'ed together */
		"\"/etc/sudoers\""
		"\"0123456789abcdef0123456789abcdef01234567\""
		"\"someuser\"" /* rf_owner */
		"\"somegroup\"" /* rf_group */
		"000001a4" /* rf_mode 0644 */
		"";
	assert_str_equals("packs properly (normal case)", expected, packed);

	res_file_free(rf);
	free(packed);
}

void test_res_file_unpack()
{
	struct res_file *rf;
	char *packed;

	packed = "res_file::"
		"00000003" /* UID and GID only */
		"\"/etc/sudoers\""
		"\"0123456789abcdef0123456789abcdef01234567\""
		"\"someuser\""  /* rf_owner */
		"\"somegroup\"" /* rf_group */
		"000001a4" /* rf_mode 0644 */
		"";

	test("RES_FILE: file unserialization");
	rf = res_file_unpack(packed);
	assert_not_null("res_file_unpack succeeds", rf);
	assert_str_equals("res_file->rf_lpath is \"/etc/sudoers\"", "/etc/sudoers", rf->rf_lpath);
	assert_str_equals("res_file->rf_rsha1.hex is \"0123456789abcdef0123456789abcdef01234567\"",
	                  "0123456789abcdef0123456789abcdef01234567", rf->rf_rsha1.hex);
	assert_true("SHA1 is NOT enforced", !res_file_enforced(rf, SHA1));

	assert_str_equals("res_file->rf_owner is \"someuser\"", "someuser", rf->rf_owner);
	assert_true("UID is enforced", res_file_enforced(rf, UID));

	assert_str_equals("Res_file->rf_group is \"somegroup\"", "somegroup", rf->rf_group);
	assert_true("GID is enforced", res_file_enforced(rf, GID));

	assert_int_equals("res_file->rf_mode is 0644", 0644, rf->rf_mode);
	assert_true("MODE is NOT enforced", !res_file_enforced(rf, MODE));

	res_file_free(rf);
}

void test_suite_res_file()
{
	test_res_file_enforcement();
	test_res_file_diffstat();
	test_res_file_remedy();
	test_res_file_fixup_new();
	test_res_file_fixup_remove_existing();
	test_res_file_fixup_remove_nonexistent();

	test_res_file_pack_detection();
	test_res_file_pack();
	test_res_file_unpack();
}
