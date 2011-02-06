#include "test.h"
#include "assertions.h"
#include "../clockwork.h"
#include "../res_user.h"

#define ASSERT_ENFORCEMENT(o,f,c,t,v) do {\
	res_user_set_ ## f (o,v); \
	assert_true( #c " enforced", res_user_enforced(o,c)); \
	assert_ ## t ## _equals( #c " set properly", (o)->ru_ ## f, v); \
} while(0)

void test_res_user_enforcement()
{
	struct res_user *ru;
	ru = res_user_new("user1");

	test("RES_USER: Default Enforcements");
	assert_true("NAME enforced",        res_user_enforced(ru, NAME));
	assert_true("PASSWD not enforced", !res_user_enforced(ru, PASSWD));
	assert_true("UID not enforced",    !res_user_enforced(ru, UID));
	assert_true("GID not enforced",    !res_user_enforced(ru, GID));

	test("RES_USER: NAME enforcement");
	ASSERT_ENFORCEMENT(ru,name,NAME,str,"name");

	test("RES_USER: PASSWD enforcement");
	ASSERT_ENFORCEMENT(ru,passwd,PASSWD,str,"pass");

	test("RES_USER: UID enforcement");
	ASSERT_ENFORCEMENT(ru,uid,UID,int,4);

	test("RES_USER: GID enforcement");
	ASSERT_ENFORCEMENT(ru,gid,GID,int,4);

	test("RES_USER: GECOS enforcement");
	ASSERT_ENFORCEMENT(ru,gecos,GECOS,str,"Comment");

	test("RES_USER: DIR enforcement");
	ASSERT_ENFORCEMENT(ru,dir,DIR,str,"/home/user");

	test("RES_USER: SHELL enforcement");
	ASSERT_ENFORCEMENT(ru,shell,SHELL,str,"/bin/bash");

	test("RES_USER: MKHOME enforcement");
	res_user_set_makehome(ru, 1, "/etc/skel.admin");
	assert_true("MKHOME enforced", res_user_enforced(ru, MKHOME));
	assert_int_equals("MKHOME set properly", ru->ru_mkhome, 1);
	assert_str_equals("SKEL set properly", ru->ru_skel, "/etc/skel.admin");

	res_user_set_makehome(ru, 0, "/etc/skel");
	assert_true("MKHOME re-re-enforced", res_user_enforced(ru, MKHOME));
	assert_int_equals("MKHOME re-re-set properly", ru->ru_mkhome, 0);
	assert_null("SKEL is NULL", ru->ru_skel);

	test("RES_USER: PWMIN enforcement");
	ASSERT_ENFORCEMENT(ru,pwmin,PWMIN,int,7);

	test("RES_USER: PWMAX enforcement");
	ASSERT_ENFORCEMENT(ru,pwmax,PWMAX,int,45);

	test("RES_USER: PWWARN enforcement");
	ASSERT_ENFORCEMENT(ru,pwwarn,PWWARN,int,2);

	test("RES_USER: INACT enforcement");
	ASSERT_ENFORCEMENT(ru,inact,INACT,int,999);

	test("RES_USER: EXPIRE enforcement");
	ASSERT_ENFORCEMENT(ru,expire,EXPIRE,int,100);

	test("RES_USER: LOCK enforcement");
	ASSERT_ENFORCEMENT(ru,lock,LOCK,int,1);

	res_user_free(ru);
}

void test_res_user_diffstat_remediation()
{
	struct res_user *ru;
	struct pwdb *pwdb;
	struct spdb *spdb;
	struct report *report;

	ru = res_user_new("svc");
	res_user_set_uid(ru, 7001);
	res_user_set_gid(ru, 8001);
	res_user_set_gecos(ru, "SVC service account");
	res_user_set_shell(ru, "/sbin/nologin");
	res_user_set_dir(ru, "/tmp/nonexistent");
	res_user_set_makehome(ru, 1, "/etc/skel");
	res_user_set_pwmin(ru, 4);
	res_user_set_pwmax(ru, 45);
	res_user_set_pwwarn(ru, 3);

	pwdb = pwdb_init("test/data/passwd");
	if (!pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	spdb = spdb_init("test/data/shadow");
	if (!spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Diffstat");
	assert_int_equals("res_user_stat returns zero", res_user_stat(ru, pwdb, spdb), 0);
	assert_true("NAME is in compliance", !res_user_different(ru, NAME));
	assert_true("UID is out of compliance", res_user_different(ru, UID));
	assert_true("GID is out of compliance", res_user_different(ru, GID));
	assert_true("GECOS is out of compliance", res_user_different(ru, GECOS));
	assert_true("SHELL is in compliance", !res_user_different(ru, SHELL));
	assert_true("DIR is in compliance", !res_user_different(ru, DIR));
	assert_true("MKHOME is out of compliance", res_user_different(ru, MKHOME));
	assert_true("PWMIN is out of compliance", res_user_different(ru, PWMIN));
	assert_true("PWMAX is out of compliance", res_user_different(ru, PWMAX));
	assert_true("PWWARN is out of compliance", res_user_different(ru, PWWARN));

	test("RES_USER: Remediation (existing account)");
	report = res_user_remediate(ru, 0, pwdb, spdb);
	assert_not_null("res_user_remediate returns a report", report);
	assert_int_equals("user is fixed", report->fixed, 1);
	assert_int_equals("user is now compliant", report->compliant, 1);

	assert_str_equals("pw_name is still set properly", ru->ru_pw->pw_name, "svc");
	assert_int_equals("pw_uid is updated properly", ru->ru_pw->pw_uid, 7001);
	assert_int_equals("pw_gid is updated properly", ru->ru_pw->pw_gid, 8001);
	assert_str_equals("pw_gecos is updated properly", ru->ru_pw->pw_gecos, "SVC service account");
	assert_str_equals("pw_shell is still set properly", ru->ru_pw->pw_shell, "/sbin/nologin");
	assert_str_equals("pw_dir is still set properly", ru->ru_pw->pw_dir, "/tmp/nonexistent");

	assert_str_equals("sp_namp is still set properly", ru->ru_sp->sp_namp, "svc");
	assert_int_equals("sp_min is still set properly", ru->ru_sp->sp_min, 4);
	assert_int_equals("sp_max is still set properly", ru->ru_sp->sp_max, 45);
	assert_int_equals("sp_warn is still set properly", ru->ru_sp->sp_warn, 3);

	res_user_free(ru);
	pwdb_free(pwdb);
	spdb_free(spdb);
	report_free(report);
}

void test_res_user_remediation_new()
{
	struct res_user *ru;
	struct pwdb *pwdb;
	struct spdb *spdb;
	struct report *report;

	ru = res_user_new("new_user");
	res_user_set_uid(ru, 7010);
	res_user_set_gid(ru, 20);
	res_user_set_gecos(ru, "New Account");
	res_user_set_shell(ru, "/sbin/nologin");
	res_user_set_dir(ru, "test/tmp/new_user.home");
	res_user_set_makehome(ru, 1, "/etc/skel.svc");

	pwdb = pwdb_init("test/data/passwd");
	if (!pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	spdb = spdb_init("test/data/shadow");
	if (!spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Remediation (new account)");
	assert_int_equals("res_user_stat returns zero", res_user_stat(ru, pwdb, spdb), 0);

	report = res_user_remediate(ru, 0, pwdb, spdb);
	assert_not_null("res_user_remediate returns a report", report);
	assert_int_equals("user is fixed", report->fixed, 1);
	assert_int_equals("user is now compliant", report->compliant, 1);

	assert_str_equals("pw_name is set properly", ru->ru_pw->pw_name, "new_user");
	assert_int_equals("pw_uid is set properly", ru->ru_pw->pw_uid, 7010);
	assert_int_equals("pw_gid is set properly", ru->ru_pw->pw_gid, 20);
	assert_str_equals("pw_gecos is set properly", ru->ru_pw->pw_gecos, "New Account");
	assert_str_equals("pw_shell is set properly", ru->ru_pw->pw_shell, "/sbin/nologin");
	assert_str_equals("pw_dir is set properly", ru->ru_pw->pw_dir, "test/tmp/new_user.home");

	assert_str_equals("sp_namp is set properly", ru->ru_sp->sp_namp, "new_user");

	res_user_free(ru);
	pwdb_free(pwdb);
	spdb_free(spdb);
	report_free(report);
}

void test_res_user_remediation_remove_existing()
{
	struct res_user *ru;
	struct pwdb *pwdb, *pwdb_after;
	struct spdb *spdb, *spdb_after;
	struct report *report;

	ru = res_user_new("sys");
	res_user_set_presence(ru, 0); /* Remove the user */

	pwdb = pwdb_init("test/data/passwd");
	if (!pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	spdb = spdb_init("test/data/shadow");
	if (!spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Remediation (remove existing account)");
	assert_int_equals("res_user_stat returns zero", res_user_stat(ru, pwdb, spdb), 0);
	assert_not_null("(test sanity) user found in passwd file", ru->ru_pw);
	assert_not_null("(test sanity) user found in shadow file", ru->ru_sp);

	report = res_user_remediate(ru, 0, pwdb, spdb);
	assert_not_null("res_user_remediate returns a report", report);
	assert_int_equals("user is fixed", report->fixed, 1);
	assert_int_equals("user is now compliant", report->compliant, 1);

	assert_int_equals("pwdb_write succeeds", 0, pwdb_write(pwdb, "test/tmp/passwd.new"));
	assert_int_equals("spdb_write succeeds", 0, spdb_write(spdb, "test/tmp/shadow.new"));

	pwdb_after = pwdb_init("test/tmp/passwd.new");
	if (!pwdb_after) {
		assert_fail("Unable to init pwdb_after");
		return;
	}

	spdb_after = spdb_init("test/tmp/shadow.new");
	if (!spdb_after) {
		assert_fail("Unable to init spdb_after");
		return;
	}

	res_user_free(ru);
	ru = res_user_new("sys");
	assert_int_equals("res_user_stat returns zero (after)", res_user_stat(ru, pwdb_after, spdb_after), 0);
	assert_null("No passwd entry exists after remediation", ru->ru_pw);
	assert_null("No shadow entry exists after remediation", ru->ru_sp);

	res_user_free(ru);
	pwdb_free(pwdb);
	spdb_free(spdb);
	pwdb_free(pwdb_after);
	spdb_free(spdb_after);
	report_free(report);
}

void test_res_user_remediation_remove_nonexistent()
{
	struct res_user *ru;
	struct pwdb *pwdb, *pwdb_after;
	struct spdb *spdb, *spdb_after;
	struct report *report;

	ru = res_user_new("non_existent_user");
	res_user_set_presence(ru, 0); /* Remove the user */

	pwdb = pwdb_init("test/data/passwd");
	if (!pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	spdb = spdb_init("test/data/shadow");
	if (!spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Remediation (remove non-existent account)");
	assert_int_equals("res_user_stat returns zero", res_user_stat(ru, pwdb, spdb), 0);
	assert_null("(test sanity) user not found in passwd file", ru->ru_pw);
	assert_null("(test sanity) user not found in shadow file", ru->ru_sp);

	report = res_user_remediate(ru, 0, pwdb, spdb);
	assert_not_null("res_user_remediate returns a report", report);
	assert_int_equals("user was already compliant", report->fixed, 0);
	assert_int_equals("user is now compliant", report->compliant, 1);

	assert_int_equals("pwdb_write succeeds", 0, pwdb_write(pwdb, "test/tmp/passwd.new"));
	assert_int_equals("spdb_write succeeds", 0, spdb_write(spdb, "test/tmp/shadow.new"));

	pwdb_after = pwdb_init("test/tmp/passwd.new");
	if (!pwdb_after) {
		assert_fail("Unable to init pwdb_after");
		return;
	}

	spdb_after = spdb_init("test/tmp/shadow.new");
	if (!spdb_after) {
		assert_fail("Unable to init spdb_after");
		return;
	}

	res_user_free(ru);
	ru = res_user_new("non_existent_user");
	assert_int_equals("res_user_stat returns zero (after)", res_user_stat(ru, pwdb_after, spdb_after), 0);
	assert_null("No passwd entry exists after remediation", ru->ru_pw);
	assert_null("No shadow entry exists after remediation", ru->ru_sp);

	res_user_free(ru);
	pwdb_free(pwdb);
	spdb_free(spdb);
	pwdb_free(pwdb_after);
	spdb_free(spdb_after);
	report_free(report);
}

void test_res_user_pack_detection()
{
	test("RES_USER: pack detection based on tag");
	assert_int_equals("With leading tag", 0, res_user_is_pack("res_user::"));
	assert_int_not_equal("Bad tag (space issue)", 0, res_user_is_pack("res_user ::"));
	assert_int_not_equal("Bad tag (res_group)", 0, res_user_is_pack("res_group::"));
	assert_int_not_equal("Bad tag (random string)", 0, res_user_is_pack("this is an ARBITRARY string"));
}

void test_res_user_pack()
{
	struct res_user *ru;
	char *packed;
	const char *expected;

	ru = res_user_new("user");
	res_user_set_uid(ru, 123); // hex: 0000007b
	res_user_set_gid(ru, 999); // hex: 000003e7
	res_user_set_passwd(ru, "sooper.seecret");
	res_user_set_dir(ru, "/home/user");
	res_user_set_gecos(ru, "GECOS for user");
	res_user_set_shell(ru, "/sbin/nologin");
	res_user_set_makehome(ru, 1, "/etc/skel.oper");
	res_user_set_pwmin(ru, 4);      // hex: 00000004
	res_user_set_pwmax(ru, 90);     // hex: 0000005a
	res_user_set_pwwarn(ru, 14);    // hex: 0000000e
	res_user_set_inact(ru, 1000);   // hex: 000003e8
	res_user_set_expire(ru, 2000);  // hex: 000007d0
	res_user_set_lock(ru, 0);

	test("RES_USER: pack res_user");
	packed = res_user_pack(ru);
	expected = "res_user::"
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
	assert_str_equals("packs properly (normal case)", expected, packed);

	res_user_free(ru);
	free(packed);
}

void test_res_user_unpack()
{
	struct res_user *ru;
	char *packed;

	packed = "res_user::"
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

	ru = res_user_unpack(packed);

	test("RES_USER: unpack res_user");
	assert_not_null("res_user_unpack succeeds", ru);
	assert_str_equals("res_user->ru_name is \"user\"", "user", ru->ru_name);
	assert_true("NAME is enforced", res_user_enforced(ru, NAME));

	assert_str_equals("res_user->ru_passwd is \"secret\"", "secret", ru->ru_passwd);
	assert_true("PASSWD is NOT enforced", !res_user_enforced(ru, PASSWD));

	assert_int_equals("res_user->ru_uid is 45", 45, ru->ru_uid);
	assert_true("UID is NOT enforced", !res_user_enforced(ru, UID));

	assert_int_equals("res_user->ru_gid is 90", 90, ru->ru_gid);
	assert_true("GID is NOT enforced", !res_user_enforced(ru, GID));

	assert_str_equals("res_user->ru_gecos is \"GECOS\"", "GECOS", ru->ru_gecos);
	assert_true("GECOS is NOT enforced", !res_user_enforced(ru, GECOS));

	assert_str_equals("res_user->ru_dir is \"/home/user\"", "/home/user", ru->ru_dir);
	assert_true("DIR is NOT enforced", !res_user_enforced(ru, DIR));

	assert_str_equals("res_user->ru_shell is \"/bin/bash\"", "/bin/bash", ru->ru_shell);
	assert_true("SHELL is NOT enforced", !res_user_enforced(ru, SHELL));

	assert_int_equals("res_user->ru_mkhome is 1", 1, ru->ru_mkhome);
	assert_str_equals("res_user->ru_skel is \"/etc/skel.oper\"", "/etc/skel.oper", ru->ru_skel);
	assert_true("MKHOME is NOT enforced", !res_user_enforced(ru, MKHOME));

	assert_int_equals("res_user->ru_lock is 0", 0, ru->ru_lock);
	assert_true("LOCK is enforced", res_user_enforced(ru, LOCK));

	assert_int_equals("res_user->ru_pwmin is 4", 4, ru->ru_pwmin);
	assert_true("PWMIN is NOT enforced", !res_user_enforced(ru, PWMIN));

	assert_int_equals("res_user->ru_pwmax is 50", 50, ru->ru_pwmax);
	assert_true("PWMAX is NOT enforced", !res_user_enforced(ru, PWMAX));

	assert_int_equals("res_user->ru_pwwarn is 7", 7, ru->ru_pwwarn);
	assert_true("PWWARN is NOT enforced", !res_user_enforced(ru, PWWARN));

	assert_int_equals("res_user->ru_inact is 6000", 6000, ru->ru_inact);
	assert_true("INACT is NOT enforced", !res_user_enforced(ru, INACT));

	assert_int_equals("res_user->ru_expire is 9000", 9000, ru->ru_expire);
	assert_true("EXPIRE is enforced", res_user_enforced(ru, EXPIRE));

	res_user_free(ru);
}

void test_suite_res_user()
{
	test_res_user_enforcement();
	test_res_user_diffstat_remediation();
	test_res_user_remediation_new();
	test_res_user_remediation_remove_existing();
	test_res_user_remediation_remove_nonexistent();

	test_res_user_pack_detection();
	test_res_user_pack();
	test_res_user_unpack();
}
