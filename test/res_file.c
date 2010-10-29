#include "test.h"
#include "sha1_files.h"
#include "../res_file.h"

#define ASSERT_ENFORCEMENT(o,f,c,t,v1,v2) do {\
	res_file_set_ ## f (o,v1); \
	assert_true( #c " enforced", res_file_enforced(o,c)); \
	assert_ ## t ## _equals( #c " set properly", (o)->rf_ ## f, v1); \
	\
	res_file_unset_ ## f (o); \
	assert_true( #c " no longer enforced", !res_file_enforced(o,c)); \
	\
	res_file_set_ ## f (o,v2); \
	assert_true( #c " re-enforced", res_file_enforced(o,c)); \
	assert_ ## t ## _equals ( #c " re-set properly", (o)->rf_ ## f, v2); \
} while(0)

static void assert_rf_source_enforcement(struct res_file *rf)
{
	const char *src1 = "test/data/sha1/file1";
	const char *src2 = "test/data/sha1/file2";

	res_file_set_source(rf, src1);
	assert_true("SHA1 enforced", res_file_enforced(rf, SHA1));
	assert_str_equals("SHA1 set properly", rf->rf_rsha1.hex, TESTFILE_SHA1_file1);
	assert_str_equals("rf_rpath set properly", rf->rf_rpath, src1);

	res_file_unset_source(rf);
	assert_true("SHA1 no longer enforced", !res_file_enforced(rf, SHA1));

	res_file_set_source(rf, src2);
	assert_str_equals("SHA1 re-set properly", rf->rf_rsha1.hex, TESTFILE_SHA1_file2);
	assert_str_equals("rf_rpath re-set properly", rf->rf_rpath, src2);
}

void test_res_file_enforcement()
{
	struct res_file rf;
	res_file_init(&rf);

	test("RES_FILE: Default Enforcements");
	assert_true("UID not enforced", !res_file_enforced(&rf, UID));
	assert_true("GID not enforced", !res_file_enforced(&rf, GID));
	assert_true("MODE not enforced", !res_file_enforced(&rf, MODE));
	assert_true("SHA1 not enforced", !res_file_enforced(&rf, SHA1));

	test("RES_FILE: UID enforcement");
	ASSERT_ENFORCEMENT(&rf,uid,UID,int,23,46);

	test("RES_FILE: GID enforcement");
	ASSERT_ENFORCEMENT(&rf,gid,GID,int,45,11);

	test("RES_FILE: MODE enforcement");
	ASSERT_ENFORCEMENT(&rf,mode,MODE,int,0755,0447);

	test("RES_FILE: SHA1 / rpath enforcement");
	assert_rf_source_enforcement(&rf);

	res_file_free(&rf);
}

void test_res_file_merge()
{
	struct res_file rf1;
	struct res_file rf2;

	res_file_init(&rf1);
	rf1.rf_prio =  10;

	res_file_init(&rf2);
	rf2.rf_prio = 100;

	/* rf1 is our "base" policy,
	   to be overridden by rf2 */
	res_file_set_uid(&rf1, 42);
	res_file_set_gid(&rf1, 42);

	res_file_set_source(&rf2, "test/data/sha1/file1");
	res_file_set_uid(&rf2, 199);
	res_file_set_gid(&rf2, 199);
	res_file_set_mode(&rf2, 0700);

	test("RES_FILE: Merging file resources together");
	res_file_merge(&rf1, &rf2);
	assert_int_equals("UID set properly after merge", rf1.rf_uid, 42);
	assert_int_equals("GID set properly after merge", rf1.rf_gid, 42);
	assert_int_equals("MODE set properly after merge", rf1.rf_mode, 0700);
	assert_str_equals("LPATH set properly after merge", rf1.rf_rpath, "test/data/sha1/file1");
	assert_str_equals("LSHA1 set properly after merge", rf1.rf_rsha1.hex, TESTFILE_SHA1_file1);

	res_file_free(&rf1);
	res_file_free(&rf2);
}

void test_res_file_diffstat()
{
	struct res_file rf;

	res_file_init(&rf);
	rf.rf_lpath = "test/data/res_file/sudoers"; /* FIXME: should we have a _set_ function for this? */

	res_file_set_uid(&rf, 42);
	res_file_set_gid(&rf, 42);
	res_file_set_mode(&rf, 0440);

	test("RES_FILE: res_file_diffstat picks up file differences");
	assert_int_equals("res_file_stat returns zero", res_file_stat(&rf), 0);
	assert_true("UID is out of compliance",  res_file_different(&rf, UID));
	assert_true("GID is out of compliance",  res_file_different(&rf, GID));
	assert_true("MODE is out of compliance", res_file_different(&rf, MODE));

	res_file_free(&rf);
}

void test_res_file_remedy()
{
	struct stat st;
	struct res_file rf;

	const char *path = "test/data/res_file/fstab";
	const char *src  = "test/data/res_file/SRC/fstab";

	test("RES_FILE: File Remediation");

	/* STAT the target file */
	if (stat(path, &st) != 0) {
		assert_fail("RES_FILE: Unable to stat pre-remediation file");
		return;
	}
	assert_int_not_equal("Pre-remediation: file owner UID is not 42", st.st_uid, 42);
	assert_int_not_equal("Pre-remediation: file group GID is not 42", st.st_gid, 42);
	assert_int_not_equal("Pre-remediation: file permissions are not 0754", st.st_mode & 07777, 0754);

	res_file_init(&rf);
	rf.rf_lpath = "test/data/res_file/fstab"; /* FIXME: should we have a _set_ function for this? */

	res_file_set_uid(&rf, 42);
	res_file_set_gid(&rf, 42);
	res_file_set_mode(&rf, 0754);
	res_file_set_source(&rf, src);

	assert_int_equals("res_file_stat succeeds", res_file_stat(&rf), 0);
	assert_int_equals("res_file_remediate succeeds", res_file_remediate(&rf), 0);

	/* STAT the remediated file */
	if (stat(path, &st) != 0) {
		assert_fail("RES_FILE: Unable to stat post-remediation file");
		return;
	}
	assert_int_equals("Post-remediation: file owner UID 42", st.st_uid, 42);
	assert_int_equals("Post-remediation: file group GID 42", st.st_gid, 42);
	assert_int_equals("Post-remediation: file permissions are 0754", st.st_mode & 07777, 0754);

	res_file_free(&rf);
}

void test_res_file_serialize()
{
	struct res_file rf;
	char *serialized;
	size_t len;
	const char *expected;

	res_file_init(&rf);

	res_file_set_uid(&rf, 101);
	res_file_set_gid(&rf, 202);
	res_file_set_mode(&rf, 0644);
	res_file_set_source(&rf, "http://example.com/sudoers");

	test("RES_FILE: file serialization");
	res_file_serialize(&rf, &serialized, &len);
	expected = "{"
			    "\"\""
			":" "\"http://example.com/sudoers\""
			":" "\"101\""
			":" "\"202\""
			":" "\"420\""
		"}";
	assert_str_equals("serialized properly (normal case)", expected, serialized);

	res_file_free(&rf);
	free(serialized);
}

void test_res_file_unserialize()
{
	struct res_file rf;
	char *serialized;
	size_t len;

	serialized = "{"
			    "\"/etc/sudoers\""
			":" "\"http://example.com/sudoers\""
			":" "\"101\""
			":" "\"202\""
			":" "\"420\""
		"}";
	len = strlen(serialized);

	res_file_init(&rf);

	test("RES_FILE: file unserialization");
	assert_int_equals("res_file_unserialize succeeds", 0, res_file_unserialize(&rf, serialized, len));
	assert_str_equals("res_file->rf_lpath is \"/etc/sudoers\"", "/etc/sudoers", rf.rf_lpath);
	assert_str_equals("res_file->rf_rpath is \"http://example.com/sudoers\"", "/etc/sudoers", rf.rf_lpath);
	assert_int_equals("res_file->rf_uid is 101", 101, rf.rf_uid);
	assert_int_equals("res_file->rf_gid is 202", 202, rf.rf_gid);
	assert_int_equals("res_file->rf_mode is 0644", 0644, rf.rf_mode);

	res_file_free(&rf);
}

void test_suite_res_file()
{
	test_res_file_enforcement();
	test_res_file_merge();
	test_res_file_diffstat();
	test_res_file_remedy();

	test_res_file_serialize();
	test_res_file_unserialize();
}
