#include "test.h"
#include "assertions.h"
#include "sha1_files.h"
#include "../clockwork.h"
#include "../resources.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

void test_resource_keys()
{
	struct res_user     *user;
	struct res_group    *group;
	struct res_file     *file;
	struct res_service  *service;
	struct res_package  *package;

	char *k;

	test("RES_*: Resource Key Generation");
	user = res_user_new("user-key");
	k = res_user_key(user);
	assert_str_equals("user key formatted properly", "res_user:user-key", k);
	free(k);
	res_user_free(user);

	group = res_group_new("group-key");
	k = res_group_key(group);
	assert_str_equals("group key formatted properly", "res_group:group-key", k);
	free(k);
	res_group_free(group);

	file = res_file_new("file-key");
	k = res_file_key(file);
	assert_str_equals("file key formatted properly", "res_file:file-key", k);
	free(k);
	res_file_free(file);

	service = res_service_new("service-key");
	k = res_service_key(service);
	assert_str_equals("service key formatted properly", "res_service:service-key", k);
	free(k);
	res_service_free(service);

	package = res_package_new("package-key");
	k = res_package_key(package);
	assert_str_equals("package key formatted properly", "res_package:package-key", k);
	free(k);
	res_package_free(package);
}

void test_resource_noops()
{
	test("RES_USER: notify is a NOOP");
	assert_int_equals("res_user_notify does nothing", 0, res_user_notify(NULL, NULL));

	test("RES_USER: norm is a NOOP");
	assert_int_equals("res_user_norm does nothing", 0, res_user_norm(NULL, NULL));

	test("RES_GROUP: notify is a NOOP");
	assert_int_equals("res_group_notify does nothing", 0, res_group_notify(NULL, NULL));

	test("RES_GROUP: norm is a NOOP");
	assert_int_equals("res_group_norm does nothing", 0, res_group_norm(NULL, NULL));

	test("RES_FILE: notify is a NOOP");
	assert_int_equals("res_file_notify does nothing", 0, res_file_notify(NULL, NULL));

	test("RES_SERVICE: norm is a NOOP");
	assert_int_equals("res_service_norm does nothing", 0, res_service_norm(NULL, NULL));

	test("RES_PACKAGE: notify is a NOOP");
	assert_int_equals("res_package_notify does nothing", 0, res_package_notify(NULL, NULL));

	test("RES_PACKAGE: norm is a NOOP");
	assert_int_equals("res_package_norm does nothing", 0, res_package_norm(NULL, NULL));


}

void test_res_user_enforcement()
{
	struct res_user *ru;
	ru = res_user_new("user1");

	test("RES_USER: Default Enforcements");
	assert_true("NAME enforced",        ENFORCED(ru, RES_USER_NAME));
	assert_true("PASSWD not enforced", !ENFORCED(ru, RES_USER_PASSWD));
	assert_true("UID not enforced",    !ENFORCED(ru, RES_USER_UID));
	assert_true("GID not enforced",    !ENFORCED(ru, RES_USER_GID));

	test("RES_USER: NAME enforcement");
	res_user_set(ru, "username", "Name");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_NAME));
	assert_str_eq("value set properly", ru->ru_name, "Name");

	test("RES_USER: PASSWD enforcement");
	res_user_set(ru, "pwhash", "!@#hash!@#");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_PASSWD));
	assert_str_eq("value set properly", ru->ru_passwd, "!@#hash!@#");

	test("RES_USER: UID enforcement");
	res_user_set(ru, "uid", "4");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_UID));
	assert_int_eq("value set properly", ru->ru_uid, 4);

	test("RES_USER: GID enforcement");
	res_user_set(ru, "gid", "5");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_GID));
	assert_int_eq("value set properly", ru->ru_gid, 5);

	test("RES_USER: GECOS enforcement");
	res_user_set(ru, "comment", "GECOS comment");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_GECOS));
	assert_str_eq("value set properly", ru->ru_gecos, "GECOS comment");

	test("RES_USER: DIR enforcement");
	res_user_set(ru, "home", "/home/user");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_DIR));
	assert_str_eq("value set properly", ru->ru_dir, "/home/user");

	test("RES_USER: SHELL enforcement");
	res_user_set(ru, "shell", "/bin/bash");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_SHELL));
	assert_str_eq("value set properly", ru->ru_shell, "/bin/bash");

	test("RES_USER: MKHOME enforcement");
	res_user_set(ru, "makehome", "/etc/skel.admin");
	assert_true("MKHOME enforced", ENFORCED(ru, RES_USER_MKHOME));
	assert_int_equals("MKHOME set properly", ru->ru_mkhome, 1);
	assert_str_equals("SKEL set properly", ru->ru_skel, "/etc/skel.admin");

	res_user_set(ru, "makehome", "no");
	assert_true("MKHOME re-re-enforced", ENFORCED(ru, RES_USER_MKHOME));
	assert_int_equals("MKHOME re-re-set properly", ru->ru_mkhome, 0);
	assert_null("SKEL is NULL", ru->ru_skel);

	test("RES_USER: PWMIN enforcement");
	res_user_set(ru, "pwmin", "7");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_PWMIN));
	assert_int_eq("value set properly", ru->ru_pwmin, 7);

	test("RES_USER: PWMAX enforcement");
	res_user_set(ru, "pwmax", "45");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_PWMAX));
	assert_int_eq("value set properly", ru->ru_pwmax, 45);

	test("RES_USER: PWWARN enforcement");
	res_user_set(ru, "pwwarn", "29");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_PWWARN));
	assert_int_eq("value set properly", ru->ru_pwwarn, 29);

	test("RES_USER: INACT enforcement");
	res_user_set(ru, "inact", "999");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_INACT));
	assert_int_eq("value set properly", ru->ru_inact, 999);

	test("RES_USER: EXPIRE enforcement");
	res_user_set(ru, "expiry", "100");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_EXPIRE));
	assert_int_eq("value set properly", ru->ru_expire, 100);

	test("RES_USER: LOCK enforcement");
	res_user_set(ru, "locked", "yes");
	assert_true("attribute enforced", ENFORCED(ru, RES_USER_LOCK));
	assert_int_eq("value set properly", ru->ru_lock, 1);

	res_user_free(ru);
}

void test_res_user_diffstat_fixup()
{
	struct res_user *ru;
	struct resource_env env;
	struct report *report;

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

	env.user_pwdb = pwdb_init("test/data/passwd");
	if (!env.user_pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	env.user_spdb = spdb_init("test/data/shadow");
	if (!env.user_spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Diffstat");
	assert_int_equals("res_user_stat returns zero", res_user_stat(ru, &env), 0);
	assert_true("NAME is in compliance", !DIFFERENT(ru, RES_USER_NAME));
	assert_true("UID is out of compliance", DIFFERENT(ru, RES_USER_UID));
	assert_true("GID is out of compliance", DIFFERENT(ru, RES_USER_GID));
	assert_true("GECOS is out of compliance", DIFFERENT(ru, RES_USER_GECOS));
	assert_true("SHELL is in compliance", !DIFFERENT(ru, RES_USER_SHELL));
	assert_true("DIR is in compliance", !DIFFERENT(ru, RES_USER_DIR));
	assert_true("MKHOME is out of compliance", DIFFERENT(ru, RES_USER_MKHOME));
	assert_true("PWMIN is out of compliance", DIFFERENT(ru, RES_USER_PWMIN));
	assert_true("PWMAX is out of compliance", DIFFERENT(ru, RES_USER_PWMAX));
	assert_true("PWWARN is out of compliance", DIFFERENT(ru, RES_USER_PWWARN));

	test("RES_USER: Fixups (existing account)");
	report = res_user_fixup(ru, 0, &env);
	assert_not_null("res_user_fixup returns a report", report);
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
	pwdb_free(env.user_pwdb);
	spdb_free(env.user_spdb);
	report_free(report);
}

void test_res_user_fixup_new()
{
	struct res_user *ru;
	struct resource_env env;
	struct report *report;

	ru = res_user_new("new_user");
	res_user_set(ru, "uid",      "7010");
	res_user_set(ru, "gid",      "20");
	res_user_set(ru, "shell",    "/sbin/nologin");
	res_user_set(ru, "comment",  "New Account");
	res_user_set(ru, "home",     "test/tmp/new_user.home");
	res_user_set(ru, "skeleton", "/etc/skel.svc");

	env.user_pwdb = pwdb_init("test/data/passwd");
	if (!env.user_pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	env.user_spdb = spdb_init("test/data/shadow");
	if (!env.user_spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Fixups (new account)");
	assert_int_equals("res_user_stat returns zero", res_user_stat(ru, &env), 0);

	report = res_user_fixup(ru, 0, &env);
	assert_not_null("res_user_fixup returns a report", report);
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
	pwdb_free(env.user_pwdb);
	spdb_free(env.user_spdb);
	report_free(report);
}

void test_res_user_fixup_remove_existing()
{
	struct res_user *ru;
	struct resource_env env, env_after;
	struct report *report;

	ru = res_user_new("sys");
	res_user_set(ru, "present", "no"); /* Remove the user */

	env.user_pwdb = pwdb_init("test/data/passwd");
	if (!env.user_pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	env.user_spdb = spdb_init("test/data/shadow");
	if (!env.user_spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Fixups (remove existing account)");
	assert_int_equals("res_user_stat returns zero", res_user_stat(ru, &env), 0);
	assert_not_null("(test sanity) user found in passwd file", ru->ru_pw);
	assert_not_null("(test sanity) user found in shadow file", ru->ru_sp);

	report = res_user_fixup(ru, 0, &env);
	assert_not_null("res_user_fixup returns a report", report);
	assert_int_equals("user is fixed", report->fixed, 1);
	assert_int_equals("user is now compliant", report->compliant, 1);

	assert_int_equals("pwdb_write succeeds", 0, pwdb_write(env.user_pwdb, "test/tmp/passwd.new"));
	assert_int_equals("spdb_write succeeds", 0, spdb_write(env.user_spdb, "test/tmp/shadow.new"));

	env_after.user_pwdb = pwdb_init("test/tmp/passwd.new");
	if (!env_after.user_pwdb) {
		assert_fail("Unable to init env_after.user_pwdb");
		return;
	}

	env_after.user_spdb = spdb_init("test/tmp/shadow.new");
	if (!env_after.user_spdb) {
		assert_fail("Unable to init env_after.user_spdb");
		return;
	}

	res_user_free(ru);
	ru = res_user_new("sys");
	assert_int_equals("res_user_stat returns zero (after)", res_user_stat(ru, &env_after), 0);
	assert_null("No passwd entry exists after fixup", ru->ru_pw);
	assert_null("No shadow entry exists after fixup", ru->ru_sp);

	res_user_free(ru);
	pwdb_free(env.user_pwdb);
	spdb_free(env.user_spdb);
	pwdb_free(env_after.user_pwdb);
	spdb_free(env_after.user_spdb);
	report_free(report);
}

void test_res_user_fixup_remove_nonexistent()
{
	struct res_user *ru;
	struct resource_env env, env_after;
	struct report *report;

	ru = res_user_new("non_existent_user");
	res_user_set(ru, "present", "no"); /* Remove the user */

	env.user_pwdb = pwdb_init("test/data/passwd");
	if (!env.user_pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	env.user_spdb = spdb_init("test/data/shadow");
	if (!env.user_spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Fixups (remove non-existent account)");
	assert_int_equals("res_user_stat returns zero", res_user_stat(ru, &env), 0);
	assert_null("(test sanity) user not found in passwd file", ru->ru_pw);
	assert_null("(test sanity) user not found in shadow file", ru->ru_sp);

	report = res_user_fixup(ru, 0, &env);
	assert_not_null("res_user_fixup returns a report", report);
	assert_int_equals("user was already compliant", report->fixed, 0);
	assert_int_equals("user is now compliant", report->compliant, 1);

	assert_int_equals("pwdb_write succeeds", 0, pwdb_write(env.user_pwdb, "test/tmp/passwd.new"));
	assert_int_equals("spdb_write succeeds", 0, spdb_write(env.user_spdb, "test/tmp/shadow.new"));

	env_after.user_pwdb = pwdb_init("test/tmp/passwd.new");
	if (!env_after.user_pwdb) {
		assert_fail("Unable to init env_after.user_pwdb");
		return;
	}

	env_after.user_spdb = spdb_init("test/tmp/shadow.new");
	if (!env_after.user_spdb) {
		assert_fail("Unable to init env_after.user_spdb");
		return;
	}

	res_user_free(ru);
	ru = res_user_new("non_existent_user");
	assert_int_equals("res_user_stat returns zero (after)", res_user_stat(ru, &env_after), 0);
	assert_null("No passwd entry exists after fixup", ru->ru_pw);
	assert_null("No shadow entry exists after fixup", ru->ru_sp);

	res_user_free(ru);
	pwdb_free(env.user_pwdb);
	spdb_free(env.user_spdb);
	pwdb_free(env_after.user_pwdb);
	spdb_free(env_after.user_spdb);
	report_free(report);
}

void test_res_user_match()
{
	struct res_user *ru;

	ru = res_user_new("key");
	res_user_set(ru, "username", "user42");
	res_user_set(ru, "uid",      "123"); /* hex: 0000007b */
	res_user_set(ru, "gid",      "999"); /* hex: 000003e7 */
	res_user_set(ru, "home",     "/home/user");
	res_user_set(ru, "password", "sooper.seecret");

	test("RES_USER: attribute matching");
	assert_int_equals("user42 matches username=user42", 0, res_user_match(ru, "username", "user42"));
	assert_int_equals("user42 matches uid=123", 0, res_user_match(ru, "uid", "123"));
	assert_int_equals("user42 matches gid=999", 0, res_user_match(ru, "gid", "999"));
	assert_int_equals("user42 matches home=/home/user", 0, res_user_match(ru, "home", "/home/user"));

	assert_int_not_equals("user42 does not match username=bob", 0, res_user_match(ru, "username", "bob"));
	assert_int_not_equals("user42 does not match uid=42", 0, res_user_match(ru, "uid", "42"));
	assert_int_not_equals("user42 does not match gid=16", 0, res_user_match(ru, "gid", "16"));
	assert_int_not_equals("user42 does not match home=/root", 0, res_user_match(ru, "home", "/root"));

	assert_int_not_equals("password is not a matchable attr", 0, res_user_match(ru, "password", "sooper.seecret"));

	res_user_free(ru);
}

void test_res_user_pack()
{
	struct res_user *ru;
	char *packed;
	const char *expected;

	ru = res_user_new("user");
	res_user_set(ru, "uid",      "123"); /* hex: 0000007b */
	res_user_set(ru, "gid",      "999"); /* hex: 000003e7 */
	res_user_set(ru, "password", "sooper.seecret");
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

	test("RES_USER: pack res_user");
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
	assert_str_equals("packs properly (normal case)", expected, packed);

	res_user_free(ru);
	free(packed);
}

void test_res_user_unpack()
{
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

	assert_null("res_user_unpack returns NULL on failure", res_user_unpack("<invalid packed data>"));

	ru = res_user_unpack(packed);

	test("RES_USER: unpack res_user");
	assert_not_null("res_user_unpack succeeds", ru);
	assert_str_equals("res_user->key is \"userkey\"", "userkey", ru->key);
	assert_str_equals("res_user->ru_name is \"user\"", "user", ru->ru_name);
	assert_true("NAME is enforced", ENFORCED(ru, RES_USER_NAME));

	assert_str_equals("res_user->ru_passwd is \"secret\"", "secret", ru->ru_passwd);
	assert_true("PASSWD is NOT enforced", !ENFORCED(ru, RES_USER_PASSWD));

	assert_int_equals("res_user->ru_uid is 45", 45, ru->ru_uid);
	assert_true("UID is NOT enforced", !ENFORCED(ru, RES_USER_UID));

	assert_int_equals("res_user->ru_gid is 90", 90, ru->ru_gid);
	assert_true("GID is NOT enforced", !ENFORCED(ru, RES_USER_GID));

	assert_str_equals("res_user->ru_gecos is \"GECOS\"", "GECOS", ru->ru_gecos);
	assert_true("GECOS is NOT enforced", !ENFORCED(ru, RES_USER_GECOS));

	assert_str_equals("res_user->ru_dir is \"/home/user\"", "/home/user", ru->ru_dir);
	assert_true("DIR is NOT enforced", !ENFORCED(ru, RES_USER_DIR));

	assert_str_equals("res_user->ru_shell is \"/bin/bash\"", "/bin/bash", ru->ru_shell);
	assert_true("SHELL is NOT enforced", !ENFORCED(ru, RES_USER_SHELL));

	assert_int_equals("res_user->ru_mkhome is 1", 1, ru->ru_mkhome);
	assert_str_equals("res_user->ru_skel is \"/etc/skel.oper\"", "/etc/skel.oper", ru->ru_skel);
	assert_true("MKHOME is NOT enforced", !ENFORCED(ru, RES_USER_MKHOME));

	assert_int_equals("res_user->ru_lock is 0", 0, ru->ru_lock);
	assert_true("LOCK is enforced", ENFORCED(ru, RES_USER_LOCK));

	assert_int_equals("res_user->ru_pwmin is 4", 4, ru->ru_pwmin);
	assert_true("PWMIN is NOT enforced", !ENFORCED(ru, RES_USER_PWMIN));

	assert_int_equals("res_user->ru_pwmax is 50", 50, ru->ru_pwmax);
	assert_true("PWMAX is NOT enforced", !ENFORCED(ru, RES_USER_PWMAX));

	assert_int_equals("res_user->ru_pwwarn is 7", 7, ru->ru_pwwarn);
	assert_true("PWWARN is NOT enforced", !ENFORCED(ru, RES_USER_PWWARN));

	assert_int_equals("res_user->ru_inact is 6000", 6000, ru->ru_inact);
	assert_true("INACT is NOT enforced", !ENFORCED(ru, RES_USER_INACT));

	assert_int_equals("res_user->ru_expire is 9000", 9000, ru->ru_expire);
	assert_true("EXPIRE is enforced", ENFORCED(ru, RES_USER_EXPIRE));

	res_user_free(ru);
}

/*****************************************************************************/

void test_res_group_enforcement()
{
	struct res_group *rg;
	rg = res_group_new("group1");

	test("RES_GROUP: Default Enforcements");
	assert_true("NAME enforced",        ENFORCED(rg, RES_GROUP_NAME));
	assert_true("PASSWD not enforced", !ENFORCED(rg, RES_GROUP_PASSWD));
	assert_true("GID not enforced",    !ENFORCED(rg, RES_GROUP_GID));

	test("RES_GROUP: NAME enforcement");
	res_group_set(rg, "name", "name1");
	assert_true("name enforced", ENFORCED(rg, RES_GROUP_NAME));
	assert_str_eq("name set poperly", rg->rg_name, "name1");

	test("RES_GROUP: PASSWD enforcement");
	res_group_set(rg, "password", "#$hash!@#$");
	assert_true("password enforced", ENFORCED(rg, RES_GROUP_PASSWD));
	assert_str_eq("password set poperly", rg->rg_passwd , "#$hash!@#$");

	test("RES_GROUP: GID enforcement");
	res_group_set(rg, "gid", "15");
	assert_true("gid enforced", ENFORCED(rg, RES_GROUP_GID));
	assert_int_eq("gid set poperly", rg->rg_gid, 15);

	test("RES_GROUP: Membership enforcement");
	res_group_enforce_members(rg, 1);
	assert_true("membership enforced", ENFORCED(rg, RES_GROUP_MEMBERS));
	res_group_enforce_members(rg, 0);
	assert_true("membership no longer enforced", !ENFORCED(rg, RES_GROUP_MEMBERS));

	test("RES_GROUP: Administrator enforcement");
	res_group_enforce_admins(rg, 1);
	assert_true("admin list enforced", ENFORCED(rg, RES_GROUP_ADMINS));
	res_group_enforce_admins(rg, 0);
	assert_true("admin list no longer enforced", !ENFORCED(rg, RES_GROUP_ADMINS));

	res_group_free(rg);
}

void test_res_group_diffstat_fixup()
{
	struct res_group *rg;
	stringlist *list; /* for gr_mem / sg_mem / sg_adm tests */
	struct report *report;
	struct resource_env env;

	rg = res_group_new("service");
	res_group_set(rg, "gid", "6000");

	/* real membership: account1, account2 */
	res_group_enforce_members(rg, 1);
	res_group_remove_member(rg, "account1");
	res_group_add_member(rg, "account2");
	res_group_add_member(rg, "account3");

	/* real admins: admin2 */
	res_group_enforce_admins(rg, 1);
	res_group_add_admin(rg, "admin1");
	res_group_remove_admin(rg, "admin2");

	env.group_grdb = grdb_init("test/data/group");
	if (!env.group_grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	env.group_sgdb = sgdb_init("test/data/gshadow");
	if (!env.group_sgdb) {
		assert_fail("Unable to init gshadow");
		return;
	}

	test("RES_GROUP: Diffstat");
	assert_int_equals("res_group_stat returns 0", res_group_stat(rg, &env), 0);

	assert_true("NAME is in compliance", !DIFFERENT(rg, RES_GROUP_NAME));
	assert_true("GID is not in compliance", DIFFERENT(rg, RES_GROUP_GID));
	assert_true("MEMBERS is not in compliance", DIFFERENT(rg, RES_GROUP_MEMBERS));
	assert_true("ADMINS is not in compliance", DIFFERENT(rg, RES_GROUP_ADMINS));

	test("RES_GROUP: Fixups (existing account)");
	report = res_group_fixup(rg, 0, &env);
	assert_not_null("res_group_fixup returns a report", report);
	assert_int_equals("group is fixed", report->fixed, 1);
	assert_int_equals("group is now compliant", report->compliant, 1);

	assert_str_equals("rg_name is still set properly", rg->rg_grp->gr_name, "service");

	list = stringlist_new(rg->rg_grp->gr_mem);
	assert_stringlist(list, "post-fixup gr_mem", 2, "account2", "account3");
	stringlist_free(list);

	assert_str_equals("sg_namp is still set properly", rg->rg_sg->sg_namp, "service");
	assert_int_equals("rg_gid is updated properly", rg->rg_grp->gr_gid, 6000);

	list = stringlist_new(rg->rg_sg->sg_mem);
	assert_stringlist(list, "post-fixup sg_mem", 2, "account2", "account3");
	stringlist_free(list);

	list = stringlist_new(rg->rg_sg->sg_adm);
	assert_stringlist(list, "post-fixup sg_adm", 1, "admin1");
	stringlist_free(list);

	res_group_free(rg);
	grdb_free(env.group_grdb);
	sgdb_free(env.group_sgdb);
	report_free(report);
}

void test_res_group_fixup_new()
{
	struct res_group *rg;
	struct report *report;
	struct resource_env env;

	rg = res_group_new("new_group");
	res_group_set(rg, "gid", "6010");

	env.group_grdb = grdb_init("test/data/group");
	if (!env.group_grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	env.group_sgdb = sgdb_init("test/data/gshadow");
	if (!env.group_sgdb) {
		assert_fail("Unable to init gshadow");
		grdb_free(env.group_grdb);
		return;
	}

	test("RES_GROUP: Fixups (new account)");
	assert_int_equals("res_group_stat returns 0", res_group_stat(rg, &env), 0);
	report = res_group_fixup(rg, 0, &env);
	assert_not_null("res_group_fixup returns a report", report);
	assert_int_equals("group is fixed", report->fixed, 1);
	assert_int_equals("group is now compliant", report->compliant, 1);

	assert_str_equals("rg_name is still set properly", rg->rg_grp->gr_name, "new_group");

	assert_str_equals("sg_namp is still set properly", rg->rg_sg->sg_namp, "new_group");
	assert_int_equals("rg_gid is updated properly", rg->rg_grp->gr_gid, 6010);

	res_group_free(rg);
	grdb_free(env.group_grdb);
	sgdb_free(env.group_sgdb);
	report_free(report);
}

void test_res_group_fixup_remove_existing()
{
	struct res_group *rg;
	struct report *report;
	struct resource_env env;
	struct resource_env env_after;

	rg = res_group_new("daemon");
	res_group_set(rg, "present", "no"); /* Remove the group */

	env.group_grdb = grdb_init("test/data/group");
	if (!env.group_grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	env.group_sgdb = sgdb_init("test/data/gshadow");
	if (!env.group_sgdb) {
		assert_fail("Unable to init gshadow");
		grdb_free(env.group_grdb);
		return;
	}

	test("RES_GROUP: Fixups (remove existing account)");
	assert_int_equals("res_group_stat returns 0", res_group_stat(rg, &env), 0);
	assert_not_null("(test sanity) group found in group file",   rg->rg_grp);
	assert_not_null("(test sanity) group found in gshadow file", rg->rg_sg);

	report = res_group_fixup(rg, 0, &env);
	assert_not_null("res_group_fixup returns a report", report);
	assert_int_equals("group is fixed", report->fixed, 1);
	assert_int_equals("group is now compliant", report->compliant, 1);

	assert_int_equals("grdb_write succeeds", 0, grdb_write(env.group_grdb, "test/tmp/group.new"));
	assert_int_equals("sgdb_write succeeds", 0, sgdb_write(env.group_sgdb, "test/tmp/gshadow.new"));

	env_after.group_grdb = grdb_init("test/tmp/group.new");
	if (!env_after.group_grdb) {
		assert_fail("Unable to init grdb_after");
		return;
	}

	env_after.group_sgdb = sgdb_init("test/tmp/gshadow.new");
	if (!env_after.group_sgdb) {
		assert_fail("Unable to init sgdb_after");
		grdb_free(env_after.group_grdb);
		return;
	}

	res_group_free(rg);
	rg = res_group_new("non_existent_group");

	assert_int_equals("res_group_stat returns zero (after)", res_group_stat(rg, &env_after), 0);
	assert_null("No group entry exists after fixup", rg->rg_grp);
	assert_null("No gshadow entry exists after fixup", rg->rg_sg);

	res_group_free(rg);

	grdb_free(env.group_grdb);
	sgdb_free(env.group_sgdb);
	grdb_free(env_after.group_grdb);
	sgdb_free(env_after.group_sgdb);
	report_free(report);
}

void test_res_group_fixup_remove_nonexistent()
{
	struct res_group *rg;
	struct report *report;
	struct resource_env env, env_after;

	rg = res_group_new("non_existent_group");
	res_group_set(rg, "present", "no"); /* Remove the group */

	env.group_grdb = grdb_init("test/data/group");
	if (!env.group_grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	env.group_sgdb = sgdb_init("test/data/gshadow");
	if (!env.group_sgdb) {
		assert_fail("Unable to init gshadow");
		grdb_free(env.group_grdb);
		return;
	}

	test("RES_GROUP: Fixups (remove nonexistent account)");
	assert_int_equals("res_group_stat returns 0", res_group_stat(rg, &env), 0);
	assert_null("(test sanity) group not found in group file",   rg->rg_grp);
	assert_null("(test sanity) group not found in gshadow file", rg->rg_sg);

	report = res_group_fixup(rg, 0, &env);
	assert_not_null("res_group_fixup returns a report", report);
	assert_int_equals("group was already compliant", report->fixed, 0);
	assert_int_equals("group is now compliant", report->compliant, 1);

	assert_int_equals("grdb_write succeeds", 0, grdb_write(env.group_grdb, "test/tmp/group.new"));
	assert_int_equals("sgdb_write succeeds", 0, sgdb_write(env.group_sgdb, "test/tmp/gshadow.new"));

	env_after.group_grdb = grdb_init("test/tmp/group.new");
	if (!env_after.group_grdb) {
		assert_fail("Unable to init grdb_after");
		return;
	}

	env_after.group_sgdb = sgdb_init("test/tmp/gshadow.new");
	if (!env_after.group_sgdb) {
		assert_fail("Unable to init sgdb_after");
		grdb_free(env_after.group_grdb);
		return;
	}

	res_group_free(rg);
	rg = res_group_new("non_existent_group");

	assert_int_equals("res_group_stat returns zero (after)", res_group_stat(rg, &env_after), 0);
	assert_null("No group entry exists after fixups", rg->rg_grp);
	assert_null("No gshadow entry exists after fixups", rg->rg_sg);

	res_group_free(rg);

	grdb_free(env.group_grdb);
	sgdb_free(env.group_sgdb);
	grdb_free(env_after.group_grdb);
	sgdb_free(env_after.group_sgdb);
	report_free(report);
}

void test_res_group_match()
{
	struct res_group *rg;

	rg = res_group_new("key");
	res_group_set(rg, "name", "group2");
	res_group_set(rg, "gid",  "1337");
	res_group_set(rg, "password", "sesame");

	test("RES_GROUP: attribute matching");
	assert_int_equals("group2 matches name=group2", 0, res_group_match(rg, "name", "group2"));
	assert_int_equals("group2 matches gid=1337", 0, res_group_match(rg, "gid", "1337"));

	assert_int_not_equals("group2 does not match name=wheel", 0, res_group_match(rg, "name", "wheel"));
	assert_int_not_equals("group2 does not match gid=6", 0, res_group_match(rg, "gid", "6"));

	assert_int_not_equals("password is not a matchable attr", 0, res_group_match(rg, "password", "sesame"));

	res_group_free(rg);
}

void test_res_group_pack()
{
	struct res_group *rg;
	char *packed;
	const char *expected;

	rg = res_group_new("staff");         /* rg_enf == 0000 0001 */
	res_group_set(rg, "password", "sesame"); /* rg_enf == 0000 0011 */
	res_group_set(rg, "gid",      "1415");   /* rg_enf == 0000 0111 */
	res_group_add_member(rg, "admin1");  /* rg_enf == 0000 1111 */
	res_group_add_member(rg, "admin2");  /* ... */
	res_group_add_member(rg, "admin3");  /* ... */
	res_group_add_member(rg, "admin4");  /* ... */
	res_group_remove_member(rg, "user"); /* ... */
	res_group_add_admin(rg, "admin1");   /* rg_enf == 0001 1111 */

	test("RES_GROUP: pack res_group");
	packed = res_group_pack(rg);
	expected = "res_group::\"staff\"0000001f\"staff\"\"sesame\"00000587\"admin1.admin2.admin3.admin4\"\"user\"\"admin1\"\"\"";
	assert_str_equals("packs properly (normal case)", expected, packed);

	res_group_free(rg);
	free(packed);
}

void test_res_group_unpack()
{
	struct res_group *rg;
	char *packed;

	packed = "res_group::\"groupkey\"0000001d\"staff\"\"sesame\"00000587\"admin1.admin2.admin3.admin4\"\"user\"\"admin1\"\"\"";

	test("RES_GROUP: Unserialization");
	assert_null("res_group_unpack returns NULL on failure", res_group_unpack("<invalid packed data>"));

	rg = res_group_unpack(packed);
	assert_not_null("res_group_unpack succeeds", rg);
	assert_str_equals("res_group->key is \"groupkey\"", "groupkey", rg->key);
	assert_str_equals("res_group->rg_name is \"staff\"", "staff", rg->rg_name);
	assert_true("'name' is enforced", ENFORCED(rg, RES_GROUP_NAME));

	assert_str_equals("res_group->rg_passwd is \"sesame\"", "sesame", rg->rg_passwd);
	assert_true("'password' is NOT enforced", !ENFORCED(rg, RES_GROUP_PASSWD));

	assert_int_equals("res_group->rg_gid is 1415", 1415, rg->rg_gid); // 1415(10) == 0587(16)
	assert_true("'gid' is enforced", ENFORCED(rg, RES_GROUP_GID));

	assert_stringlist(rg->rg_mem_add, "res_group->rg_mem_add", 4, "admin1", "admin2", "admin3", "admin4");
	assert_stringlist(rg->rg_mem_rm,  "res_group->rg_mem_rm",  1, "user");
	assert_true("'members' is enforced", ENFORCED(rg, RES_GROUP_MEMBERS));

	assert_stringlist(rg->rg_adm_add, "res_group->rg_adm_add", 1, "admin1");
	assert_stringlist(rg->rg_adm_rm,  "res_group->rg_adm_rm",  0);
	assert_true("'admins' is enforced", ENFORCED(rg, RES_GROUP_ADMINS));

	res_group_free(rg);
}

void test_res_group_add_remove_members_via_set()
{
	struct res_group *rg;

	rg = res_group_new("g1");

	test("RES_GROUP: Adding and removing people via res_group_set()");

	res_group_set(rg, "member", "user1");
	res_group_set(rg, "member", "!user2");
	res_group_set(rg, "member", "!user3");
	res_group_set(rg, "member", "user3"); /* should move from _rm to _add */
	res_group_set(rg, "member", "user4");
	res_group_set(rg, "member", "!user4"); /* should move from _add to _rm */

	res_group_set(rg, "admin", "!admin1");
	res_group_set(rg, "admin", "admin2");
	res_group_set(rg, "admin", "!admin2"); /* should move from _add to rm */
	res_group_set(rg, "admin", "admin3");
	res_group_set(rg, "admin", "!admin4");
	res_group_set(rg, "admin", "admin4"); /* should move from _rm to _add */

	assert_stringlist(rg->rg_mem_add, "Members to add list",    2, "user1",  "user3");
	assert_stringlist(rg->rg_mem_rm,  "Members to remove list", 2, "user2",  "user4");
	assert_stringlist(rg->rg_adm_add, "Admins to add list",     2, "admin3", "admin4");
	assert_stringlist(rg->rg_adm_rm,  "Admins to remove list",  2, "admin1", "admin2");

	res_group_free(rg);
}

/*****************************************************************************/

void test_res_file_enforcement()
{
	struct res_file *rf;
	rf = res_file_new("sudoers");
	const char *src = "test/data/sha1/file1";

	test("RES_FILE: Default Enforcements");
	assert_true("UID not enforced",  !ENFORCED(rf, RES_FILE_UID));
	assert_true("GID not enforced",  !ENFORCED(rf, RES_FILE_GID));
	assert_true("MODE not enforced", !ENFORCED(rf, RES_FILE_MODE));
	assert_true("SHA1 not enforced", !ENFORCED(rf, RES_FILE_SHA1));

	test("RES_FILE: UID enforcement");
	res_file_set(rf, "owner", "someone");
	assert_true("owner enforced", ENFORCED(rf, RES_FILE_UID));
	assert_str_equals("owner set properly", rf->rf_owner, "someone");

	test("RES_FILE: GID enforcement");
	res_file_set(rf, "group", "somegroup");
	assert_true("group enforced", ENFORCED(rf, RES_FILE_GID));
	assert_str_equals("group set properly", rf->rf_group, "somegroup");

	test("RES_FILE: MODE enforcement");
	res_file_set(rf,"mode", "0755");
	assert_true("mode enforced", ENFORCED(rf, RES_FILE_MODE));
	assert_int_equals("mode set properly", rf->rf_mode, 0755);

	test("RES_FILE: SHA1 / rpath enforcement");
	res_file_set(rf, "source", src);
	assert_true("SHA1 enforced", ENFORCED(rf, RES_FILE_SHA1));
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
	assert_true("UID is out of compliance",  DIFFERENT(rf, RES_FILE_UID));
	assert_true("GID is out of compliance",  DIFFERENT(rf, RES_FILE_GID));
	assert_true("MODE is out of compliance", DIFFERENT(rf, RES_FILE_MODE));

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
	assert_true("UID is out of compliance",  DIFFERENT(rf, RES_FILE_UID));
	assert_true("GID is out of compliance",  DIFFERENT(rf, RES_FILE_GID));
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

void test_res_file_match()
{
	struct res_file *rf;

	rf = res_file_new("SUDO");

	res_file_set(rf, "owner",  "someuser");
	res_file_set(rf, "path",   "/etc/sudoers");
	res_file_set(rf, "source", "/etc/issue");

	test("RES_FILE: attribute matching");
	assert_int_equals("SUDO matches path=/etc/sudoers", 0, res_file_match(rf, "path", "/etc/sudoers"));

	assert_int_not_equals("SUDO does not match path=/tmp/wrong/file", 0, res_file_match(rf, "path", "/tmp/wrong/file"));

	assert_int_not_equals("owner is not a matchable attr", 0, res_file_match(rf, "owner", "someuser"));

	res_file_free(rf);
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
	expected = "res_file::\"/etc/sudoers\""
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

	packed = "res_file::\"somefile\""
		"00000003" /* UID and GID only */
		"\"/etc/sudoers\""
		"\"0123456789abcdef0123456789abcdef01234567\""
		"\"someuser\""  /* rf_owner */
		"\"somegroup\"" /* rf_group */
		"000001a4" /* rf_mode 0644 */
		"";

	test("RES_FILE: file unserialization");
	assert_null("res_file_unpack returns NULL on failure", res_file_unpack("<invalid packed data>"));

	rf = res_file_unpack(packed);
	assert_not_null("res_file_unpack succeeds", rf);
	assert_str_equals("res_file->key is \"somefile\"", "somefile", rf->key);
	assert_str_equals("res_file->rf_lpath is \"/etc/sudoers\"", "/etc/sudoers", rf->rf_lpath);
	assert_str_equals("res_file->rf_rsha1.hex is \"0123456789abcdef0123456789abcdef01234567\"",
	                  "0123456789abcdef0123456789abcdef01234567", rf->rf_rsha1.hex);
	assert_true("SHA1 is NOT enforced", !ENFORCED(rf, RES_FILE_SHA1));

	assert_str_equals("res_file->rf_owner is \"someuser\"", "someuser", rf->rf_owner);
	assert_true("UID is enforced", ENFORCED(rf, RES_FILE_UID));

	assert_str_equals("Res_file->rf_group is \"somegroup\"", "somegroup", rf->rf_group);
	assert_true("GID is enforced", ENFORCED(rf, RES_FILE_GID));

	assert_int_equals("res_file->rf_mode is 0644", 0644, rf->rf_mode);
	assert_true("MODE is NOT enforced", !ENFORCED(rf, RES_FILE_MODE));

	res_file_free(rf);
}

/*****************************************************************************/

void test_res_package_pack()
{
	struct res_package *r;
	char *expected = "res_package::\"pkg-name\""
	                 "00000000"
	                 "\"pkg-name\""
	                 "\"1.2.3-5\"";
	char *actual;

	test("RES_PACKAGE: package serialization");
	r = res_package_new("pkg-name");
	assert_not_null("(test sanity) Package to work with can't be NULL", r);
	res_package_set(r, "version", "1.2.3-5");

	actual = res_package_pack(r);
	assert_not_null("res_package_pack succeeds", actual);
	assert_str_eq("Packed format", actual, expected);

	res_package_free(r);
	free(actual);
}

void test_res_package_unpack()
{
	struct res_package *r;
	char *packed;

	packed = "res_package::\"pkgkey\""
		"00000000"
		"\"libtao-dev\""
		"\"5.6.3\"";

	test("RES_PACKAGE: package unserialization");
	assert_null("res_package_unpack returns NULL on failure", res_package_unpack("<invalid packed data>"));

	r = res_package_unpack(packed);
	assert_not_null("res_package_unpack succeeds", r);
	assert_str_equals("res_package->key is \"pkgkey\"", "pkgkey", r->key);
	assert_str_equals("res_package->name is \"libtao-dev\"", "libtao-dev", r->name);
	assert_str_equals("res_package->version is \"5.6.3\"", "5.6.3", r->version);
	res_package_free(r);

	packed = "res_package::\"pkgkey\""
		"00000000"
		"\"libtao-dev\""
		"\"\"";

	r = res_package_unpack(packed);
	assert_not_null("res_package_unpack [2] succeeds", r);
	assert_null("res_package->version is NULL (empty string = NULL)", r->version);
	res_package_free(r);
}

void test_res_service_pack()
{
	struct res_service *r;
	char *packed;
	const char *expected;

	r = res_service_new("svckey");
	res_service_set(r, "service", "service-name");
	res_service_set(r, "running", "yes");
	res_service_set(r, "enabled", "yes");

	test("RES_SERVICE: service serialization");
	packed = res_service_pack(r);
	expected = "res_service::\"svckey\""
		"00000005"
		"\"service-name\"";

	assert_str_equals("packs properly (normal case)", expected, packed);
	res_service_free(r);
	free(packed);
}

void test_res_service_unpack()
{
	struct res_service *r;
	char *packed;
	
	packed = "res_service::\"svckey\""
		"00000005"
		"\"service-name\"";

	test("RES_SERVICE: service unserialization");
	assert_null("res_service_unpack returns NULL on failure", res_service_unpack("<invalid packed data>"));

	r = res_service_unpack(packed);
	assert_not_null("res_service_unpack succeeds", r);
	assert_str_equals("res_service->key is \"svckey\"", "svckey", r->key);
	assert_str_equals("res_service->service is \"service-name\"", "service-name", r->service);
	assert_true("RUNNING is enforced", ENFORCED(r, RES_SERVICE_RUNNING));
	assert_false("STOPPED is NOT enforced", ENFORCED(r, RES_SERVICE_STOPPED));
	assert_true("ENABLED is enforced", ENFORCED(r, RES_SERVICE_ENABLED));
	assert_false("DISABLED is NOT enforced", ENFORCED(r, RES_SERVICE_DISABLED));

	res_service_free(r);
}

/*****************************************************************************/

void test_suite_resources()
{
	test_resource_keys();
	test_resource_noops();

	test_res_user_enforcement();
	test_res_user_diffstat_fixup();
	test_res_user_fixup_new();
	test_res_user_fixup_remove_existing();
	test_res_user_fixup_remove_nonexistent();
	test_res_user_match();
	test_res_user_pack();
	test_res_user_unpack();

	test_res_group_enforcement();
	test_res_group_diffstat_fixup();
	test_res_group_fixup_new();
	test_res_group_fixup_remove_existing();
	test_res_group_fixup_remove_nonexistent();
	test_res_group_match();
	test_res_group_pack();
	test_res_group_unpack();
	test_res_group_add_remove_members_via_set();

	test_res_file_enforcement();
	test_res_file_diffstat();
	test_res_file_remedy();
	test_res_file_fixup_new();
	test_res_file_fixup_remove_existing();
	test_res_file_fixup_remove_nonexistent();
	test_res_file_match();
	test_res_file_pack();
	test_res_file_unpack();

	test_res_package_pack();
	test_res_package_unpack();

	test_res_service_pack();
	test_res_service_unpack();
}
