#include "test.h"
#include "assertions.h"
#include "sha1_files.h"
#include "../res_file.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>


#define ASSERT_ENFORCEMENT(o,f,c,t,v) do {\
	res_file_set_ ## f (o,v); \
	assert_true( #c " enforced", res_file_enforced(o,c)); \
	assert_ ## t ## _equals( #c " set properly", (o)->rf_ ## f, v); \
} while(0)

static void assert_rf_source_enforcement(struct res_file *rf)
{
	const char *src = "test/data/sha1/file1";

	res_file_set_source(rf, src);
	assert_true("SHA1 enforced", res_file_enforced(rf, SHA1));
	assert_str_equals("SHA1 set properly", rf->rf_rsha1.hex, TESTFILE_SHA1_file1);
	assert_str_equals("rf_rpath set properly", rf->rf_rpath, src);
}

static void assert_rf_path_enforcement(struct res_file *rf)
{
	const char *src = "test/data/sha1/file1";

	res_file_set_path(rf, src);
	assert_str_equals("rf_lpath set properly", rf->rf_lpath, src);
}

void test_res_file_enforcement()
{
	struct res_file *rf;
	rf = res_file_new("sudoers");

	test("RES_FILE: Default Enforcements");
	assert_true("UID not enforced",  !res_file_enforced(rf, UID));
	assert_true("GID not enforced",  !res_file_enforced(rf, GID));
	assert_true("MODE not enforced", !res_file_enforced(rf, MODE));
	assert_true("SHA1 not enforced", !res_file_enforced(rf, SHA1));

	test("RES_FILE: UID enforcement");
	ASSERT_ENFORCEMENT(rf,owner,UID,str,"someone");

	test("RES_FILE: GID enforcement");
	ASSERT_ENFORCEMENT(rf,group,GID,str,"somegroup");

	test("RES_FILE: MODE enforcement");
	ASSERT_ENFORCEMENT(rf,mode,MODE,int,0755);

	test("RES_FILE: SHA1 / rpath enforcement");
	assert_rf_source_enforcement(rf);

	test("RES_FILE: SHA1 / lpath 'enforcement'");
	assert_rf_path_enforcement(rf);

	res_file_free(rf);
}

void test_res_file_diffstat()
{
	struct res_file *rf;

	rf = res_file_new("sudoers");
	res_file_set_path(rf, "test/data/res_file/sudoers");
	res_file_set_owner(rf, "someuser");
	rf->rf_uid = 1001;
	res_file_set_group(rf, "somegroup");
	rf->rf_gid = 2002;
	res_file_set_mode(rf, 0440);

	test("RES_FILE: res_file_diffstat picks up file differences");
	assert_int_equals("res_file_stat returns zero", res_file_stat(rf), 0);
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

	struct report *report;

	const char *path = "test/data/res_file/fstab";
	const char *src  = "test/data/res_file/SRC/fstab";

	int src_fd;
	ssize_t src_len;

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
	res_file_set_path(rf, "test/data/res_file/fstab");
	res_file_set_owner(rf, "someuser");
	rf->rf_uid = 65542;
	res_file_set_group(rf, "somegroup");
	rf->rf_gid = 65524;
	res_file_set_mode(rf, 0754);
	res_file_set_source(rf, src);

	/* set up the src_fd for content remediation */
	src_fd = open(src, O_RDONLY);
	assert_int_greater_than("(test sanity) able to open 'remote' file", src_fd, 0);
	src_len = st.st_size;

	assert_int_equals("res_file_stat succeeds", res_file_stat(rf), 0);
	assert_int_equals("File exists", 1, rf->rf_exists);
	assert_true("UID is out of compliance",  res_file_different(rf, UID));
	assert_true("GID is out of compliance",  res_file_different(rf, GID));
	report = res_file_remediate(rf, 0, src_fd, src_len);
	assert_not_null("res_file_remediate returns a report", report);
	assert_int_equals("file was fixed", report->fixed, 1);
	assert_int_equals("file is now compliant", report->compliant, 1);

	/* STAT the remediated file */
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

void test_res_file_remediate_new()
{
	struct stat st;
	struct res_file *rf;
	struct report *report;

	const char *path = "test/data/res_file/new_file";

	test("RES_FILE: File Remediation (new file)");
	rf = res_file_new(path);
	res_file_set_owner(rf, "someuser");
	rf->rf_uid = 65542;
	res_file_set_group(rf, "somegroup");
	rf->rf_gid = 65524;
	res_file_set_mode(rf, 0754);

	assert_int_equals("res_file_stat succeeds", res_file_stat(rf), 0);
	assert_int_equals("File does not already exist", 0, rf->rf_exists);
	assert_int_not_equal("stat pre-remediation file returns non-zero", 0, stat(path, &st));

	report = res_file_remediate(rf, 0, -1, 0);
	assert_not_null("res_file_remediate returns a report", report);
	assert_int_equals("file is fixed", report->fixed, 1);
	assert_int_equals("file is now compliant", report->compliant, 1);
	assert_int_equals("stat post-remediation file returns zero", 0, stat(path, &st));

	/* STAT the remediated file */
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

void test_res_file_remediate_remove_existing()
{
	struct stat st;
	struct res_file *rf;
	struct report *report;

	const char *path = "test/data/res_file/delete";

	test("RES_FILE: File Remediation (remove existing file)");
	rf = res_file_new(path);
	res_file_set_presence(rf, 0); /* Remove the file */

	assert_int_equals("res_file_stat succeeds", res_file_stat(rf), 0);
	assert_int_equals("File exists", 1, rf->rf_exists);
	assert_int_equals("stat pre-remediation file returns zero", 0, stat(path, &st));

	report = res_file_remediate(rf, 0, -1, 0);
	assert_not_null("res_file_remediate returns a report", report);
	assert_int_equals("file is fixed", report->fixed, 1);
	assert_int_equals("file is now compliant", report->compliant, 1);
	assert_int_not_equal("stat post-remediation file returns non-zero", 0, stat(path, &st));

	res_file_free(rf);
	report_free(report);
}

void test_res_file_remediate_remove_nonexistent()
{
	struct stat st;
	struct res_file *rf;
	struct report *report;

	const char *path = "test/data/res_file/non-existent";

	test("RES_FILE: File Remediation (remove non-existent file)");
	rf = res_file_new(path);
	res_file_set_presence(rf, 0); /* Remove the file */

	assert_int_equals("res_file_stat succeeds", res_file_stat(rf), 0);
	assert_int_equals("File does not already exist", 0, rf->rf_exists);
	assert_int_not_equal("stat pre-remediation file returns non-zero", 0, stat(path, &st));

	report = res_file_remediate(rf, 0, -1, 0);
	assert_not_null("res_file_remediate returns a report", report);
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

	res_file_set_owner(rf, "someuser");                     /* rf_enf == 0000 0001 */
	res_file_set_group(rf, "somegroup");                    /* rf_enf == 0000 0011 */
	res_file_set_mode(rf, 0644);                            /* rf_enf == 0000 0111 */
	res_file_set_source(rf, "/etc/issue");                  /* rf_enf == 0000 1111 */
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
	test_res_file_remediate_new();
	test_res_file_remediate_remove_existing();
	test_res_file_remediate_remove_nonexistent();

	test_res_file_pack_detection();
	test_res_file_pack();
	test_res_file_unpack();
}
