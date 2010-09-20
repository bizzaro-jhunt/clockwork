#include "test.h"
#include "sha1_files.h"
#include "../res_file.h"

static void assert_rf_uid_enforcement(struct res_file *rf)
{
	res_file_set_uid(rf, 23);
	assert_true("UID enforced", res_file_enforced(rf, UID));
	assert_int_equals("UID set properly", rf->rf_uid, 23);

	res_file_unset_uid(rf);
	assert_true("UID no longer enforced", !res_file_enforced(rf, UID));

	res_file_set_uid(rf, 46);
	assert_int_equals("UID re-set properly", rf->rf_uid, 46);
}

static void assert_rf_gid_enforcement(struct res_file *rf)
{
	res_file_set_gid(rf, 45);
	assert_true("GID enforced", res_file_enforced(rf, GID));
	assert_int_equals("GID set properly", rf->rf_gid, 45);

	res_file_unset_gid(rf);
	assert_true("GID no longer enforced", !res_file_enforced(rf, GID));

	res_file_set_gid(rf, 11);
	assert_int_equals("GID re-set properly", rf->rf_gid, 11);
}

static void assert_rf_mode_enforcement(struct res_file *rf)
{
	res_file_set_mode(rf, 0741);
	assert_true("MODE enforced", res_file_enforced(rf, MODE));
	assert_int_equals("MODE set properly", rf->rf_mode, 0741);

	res_file_unset_mode(rf);
	assert_true("MODE no longer enforced", !res_file_enforced(rf, MODE));

	res_file_set_mode(rf, 0447);
	assert_int_equals("MODE re-set properly", rf->rf_mode, 0447);
}

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
	assert_rf_uid_enforcement(&rf);

	test("RES_FILE: GID enforcement");
	assert_rf_gid_enforcement(&rf);

	test("RES_FILE: MODE enforcement");
	assert_rf_mode_enforcement(&rf);

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

void test_suite_res_file()
{
	test_res_file_enforcement();
	test_res_file_merge();
}
