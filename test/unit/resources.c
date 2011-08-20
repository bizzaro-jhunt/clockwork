#include "test.h"
#include "assertions.h"
#include "sha1_files.h"
#include "../../clockwork.h"
#include "../../resources.h"
#include "../../augcw.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

static void assert_attr_hash(const char *obj, struct hash *h, const char *attr, const char *val)
{
	char buf[128];
	snprintf(buf, 128, "%s.%s == '%s'", obj, attr, val);
	assert_str_eq(buf, val, hash_get(h, attr));
}

/***********************************************************************/

void test_resource_keys()
{
	struct res_user     *user;
	struct res_group    *group;
	struct res_file     *file;
	struct res_service  *service;
	struct res_package  *package;
	struct res_dir      *dir;
	struct res_host     *host;
	struct res_sysctl   *sysctl;

	char *k;

	test("RES_*: Resource Key Generation");
	user = res_user_new("user-key");
	k = res_user_key(user);
	assert_str_eq("user key formatted properly", "user:user-key", k);
	free(k);
	res_user_free(user);

	group = res_group_new("group-key");
	k = res_group_key(group);
	assert_str_eq("group key formatted properly", "group:group-key", k);
	free(k);
	res_group_free(group);

	file = res_file_new("file-key");
	k = res_file_key(file);
	assert_str_eq("file key formatted properly", "file:file-key", k);
	free(k);
	res_file_free(file);

	service = res_service_new("service-key");
	k = res_service_key(service);
	assert_str_eq("service key formatted properly", "service:service-key", k);
	free(k);
	res_service_free(service);

	package = res_package_new("package-key");
	k = res_package_key(package);
	assert_str_eq("package key formatted properly", "package:package-key", k);
	free(k);
	res_package_free(package);

	dir = res_dir_new("/path/to/dir");
	k = res_dir_key(dir);
	assert_str_eq("dir key formatted properly", "dir:/path/to/dir", k);
	free(k);
	res_dir_free(dir);

	host = res_host_new("localhost");
	k = res_host_key(host);
	assert_str_eq("host key formatted properly", "host:localhost", k);
	free(k);
	res_host_free(host);

	sysctl = res_sysctl_new("kernel.param1");
	k = res_sysctl_key(sysctl);
	assert_str_eq("sysctl key formatted properly", "sysctl:kernel.param1", k);
	free(k);
	res_sysctl_free(sysctl);
}

void test_resource_noops()
{
	test("RES_*: NOOPs");
	assert_int_eq("res_user_notify does nothing", 0, res_user_notify(NULL, NULL));
	assert_int_eq("res_user_norm does nothing", 0, res_user_norm(NULL, NULL, NULL));

	assert_int_eq("res_group_notify does nothing", 0, res_group_notify(NULL, NULL));
	assert_int_eq("res_group_norm does nothing", 0, res_group_norm(NULL, NULL, NULL));

	assert_int_eq("res_file_notify does nothing", 0, res_file_notify(NULL, NULL));

	assert_int_eq("res_service_norm does nothing", 0, res_service_norm(NULL, NULL, NULL));

	assert_int_eq("res_package_notify does nothing", 0, res_package_notify(NULL, NULL));
	assert_int_eq("res_package_norm does nothing", 0, res_package_norm(NULL, NULL, NULL));

	assert_int_eq("res_dir_notify does nothing", 0, res_dir_notify(NULL, NULL));
	assert_int_eq("res_sysctl_notify does nothing", 0, res_sysctl_notify(NULL, NULL));

	assert_int_eq("res_sysctl_norm does nothing", 0, res_sysctl_norm(NULL, NULL, NULL));

	assert_int_eq("res_host_notify does nothing", 0, res_host_notify(NULL, NULL));
	assert_int_eq("res_host_norm does nothing", 0, res_host_norm(NULL, NULL, NULL));
}

void test_resource_free_null()
{
	test("RES_*: free(NULL)");

	res_user_free(NULL);
	assert_true("res_user_free(NULL) doesn't segfault", 1);

	res_group_free(NULL);
	assert_true("res_group_free(NULL) doesn't segfault", 1);

	res_file_free(NULL);
	assert_true("res_file_free(NULL) doesn't segfault", 1);

	res_service_free(NULL);
	assert_true("res_service_free(NULL) doesn't segfault", 1);

	res_package_free(NULL);
	assert_true("res_package_free(NULL) doesn't segfault", 1);

	res_dir_free(NULL);
	assert_true("res_dir_free(NULL) doesn't segfault", 1);

	res_sysctl_free(NULL);
	assert_true("res_sysctl_free(NULL) doesn't segfault", 1);

	res_host_free(NULL);
	assert_true("res_host_free(NULL) doesn't segfault", 1);
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
	assert_int_eq("MKHOME set properly", ru->ru_mkhome, 1);
	assert_str_eq("SKEL set properly", ru->ru_skel, "/etc/skel.admin");

	res_user_set(ru, "makehome", "no");
	assert_true("MKHOME re-re-enforced", ENFORCED(ru, RES_USER_MKHOME));
	assert_int_eq("MKHOME re-re-set properly", ru->ru_mkhome, 0);
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

	env.user_pwdb = pwdb_init(DATAROOT "/passwd");
	if (!env.user_pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	env.user_spdb = spdb_init(DATAROOT "/shadow");
	if (!env.user_spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Diffstat");
	assert_int_eq("res_user_stat returns zero", res_user_stat(ru, &env), 0);
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
	assert_int_eq("user is fixed", report->fixed, 1);
	assert_int_eq("user is now compliant", report->compliant, 1);

	assert_str_eq("pw_name is still set properly", ru->ru_pw->pw_name, "svc");
	assert_int_eq("pw_uid is updated properly", ru->ru_pw->pw_uid, 7001);
	assert_int_eq("pw_gid is updated properly", ru->ru_pw->pw_gid, 8001);
	assert_str_eq("pw_gecos is updated properly", ru->ru_pw->pw_gecos, "SVC service account");
	assert_str_eq("pw_shell is still set properly", ru->ru_pw->pw_shell, "/sbin/nologin");
	assert_str_eq("pw_dir is still set properly", ru->ru_pw->pw_dir, "/tmp/nonexistent");

	assert_str_eq("sp_namp is still set properly", ru->ru_sp->sp_namp, "svc");
	assert_int_eq("sp_min is still set properly", ru->ru_sp->sp_min, 4);
	assert_int_eq("sp_max is still set properly", ru->ru_sp->sp_max, 45);
	assert_int_eq("sp_warn is still set properly", ru->ru_sp->sp_warn, 3);

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
	res_user_set(ru, "home",     TMPROOT "/new_user.home");
	res_user_set(ru, "skeleton", "/etc/skel.svc");

	env.user_pwdb = pwdb_init(DATAROOT "/passwd");
	if (!env.user_pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	env.user_spdb = spdb_init(DATAROOT "/shadow");
	if (!env.user_spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Fixups (new account)");
	assert_int_eq("res_user_stat returns zero", res_user_stat(ru, &env), 0);

	report = res_user_fixup(ru, 0, &env);
	assert_not_null("res_user_fixup returns a report", report);
	assert_int_eq("user is fixed", report->fixed, 1);
	assert_int_eq("user is now compliant", report->compliant, 1);

	assert_str_eq("pw_name is set properly", ru->ru_pw->pw_name, "new_user");
	assert_int_eq("pw_uid is set properly", ru->ru_pw->pw_uid, 7010);
	assert_int_eq("pw_gid is set properly", ru->ru_pw->pw_gid, 20);
	assert_str_eq("pw_gecos is set properly", ru->ru_pw->pw_gecos, "New Account");
	assert_str_eq("pw_shell is set properly", ru->ru_pw->pw_shell, "/sbin/nologin");
	assert_str_eq("pw_dir is set properly", ru->ru_pw->pw_dir, TMPROOT "/new_user.home");

	assert_str_eq("sp_namp is set properly", ru->ru_sp->sp_namp, "new_user");

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

	env.user_pwdb = pwdb_init(DATAROOT "/passwd");
	if (!env.user_pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	env.user_spdb = spdb_init(DATAROOT "/shadow");
	if (!env.user_spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Fixups (remove existing account)");
	assert_int_eq("res_user_stat returns zero", res_user_stat(ru, &env), 0);
	assert_not_null("(test sanity) user found in passwd file", ru->ru_pw);
	assert_not_null("(test sanity) user found in shadow file", ru->ru_sp);

	report = res_user_fixup(ru, 0, &env);
	assert_not_null("res_user_fixup returns a report", report);
	assert_int_eq("user is fixed", report->fixed, 1);
	assert_int_eq("user is now compliant", report->compliant, 1);

	assert_int_eq("pwdb_write succeeds", 0, pwdb_write(env.user_pwdb, TMPROOT "/passwd.new"));
	assert_int_eq("spdb_write succeeds", 0, spdb_write(env.user_spdb, TMPROOT "/shadow.new"));

	env_after.user_pwdb = pwdb_init(TMPROOT "/passwd.new");
	if (!env_after.user_pwdb) {
		assert_fail("Unable to init env_after.user_pwdb");
		return;
	}

	env_after.user_spdb = spdb_init(TMPROOT "/shadow.new");
	if (!env_after.user_spdb) {
		assert_fail("Unable to init env_after.user_spdb");
		return;
	}

	res_user_free(ru);
	ru = res_user_new("sys");
	assert_int_eq("res_user_stat returns zero (after)", res_user_stat(ru, &env_after), 0);
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

	env.user_pwdb = pwdb_init(DATAROOT "/passwd");
	if (!env.user_pwdb) {
		assert_fail("Unable to init pwdb");
		return;
	}

	env.user_spdb = spdb_init(DATAROOT "/shadow");
	if (!env.user_spdb) {
		assert_fail("Unable to init spdb");
		return;
	}

	test("RES_USER: Fixups (remove non-existent account)");
	assert_int_eq("res_user_stat returns zero", res_user_stat(ru, &env), 0);
	assert_null("(test sanity) user not found in passwd file", ru->ru_pw);
	assert_null("(test sanity) user not found in shadow file", ru->ru_sp);

	report = res_user_fixup(ru, 0, &env);
	assert_not_null("res_user_fixup returns a report", report);
	assert_int_eq("user was already compliant", report->fixed, 0);
	assert_int_eq("user is now compliant", report->compliant, 1);

	assert_int_eq("pwdb_write succeeds", 0, pwdb_write(env.user_pwdb, TMPROOT "/passwd.new"));
	assert_int_eq("spdb_write succeeds", 0, spdb_write(env.user_spdb, TMPROOT "/shadow.new"));

	env_after.user_pwdb = pwdb_init(TMPROOT "/passwd.new");
	if (!env_after.user_pwdb) {
		assert_fail("Unable to init env_after.user_pwdb");
		return;
	}

	env_after.user_spdb = spdb_init(TMPROOT "/shadow.new");
	if (!env_after.user_spdb) {
		assert_fail("Unable to init env_after.user_spdb");
		return;
	}

	res_user_free(ru);
	ru = res_user_new("non_existent_user");
	assert_int_eq("res_user_stat returns zero (after)", res_user_stat(ru, &env_after), 0);
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
	assert_int_eq("user42 matches username=user42", 0, res_user_match(ru, "username", "user42"));
	assert_int_eq("user42 matches uid=123", 0, res_user_match(ru, "uid", "123"));
	assert_int_eq("user42 matches gid=999", 0, res_user_match(ru, "gid", "999"));
	assert_int_eq("user42 matches home=/home/user", 0, res_user_match(ru, "home", "/home/user"));

	assert_int_ne("user42 does not match username=bob", 0, res_user_match(ru, "username", "bob"));
	assert_int_ne("user42 does not match uid=42", 0, res_user_match(ru, "uid", "42"));
	assert_int_ne("user42 does not match gid=16", 0, res_user_match(ru, "gid", "16"));
	assert_int_ne("user42 does not match home=/root", 0, res_user_match(ru, "home", "/root"));

	assert_int_ne("password is not a matchable attr", 0, res_user_match(ru, "password", "sooper.seecret"));

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
	assert_str_eq("packs properly (normal case)", expected, packed);

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

	test("RES_USER: unpack res_user");
	assert_null("res_user_unpack returns NULL on failure", res_user_unpack("<invalid packed data>"));

	ru = res_user_unpack(packed);

	assert_not_null("res_user_unpack succeeds", ru);
	assert_str_eq("res_user->key is \"userkey\"", "userkey", ru->key);
	assert_str_eq("res_user->ru_name is \"user\"", "user", ru->ru_name);
	assert_true("NAME is enforced", ENFORCED(ru, RES_USER_NAME));

	assert_str_eq("res_user->ru_passwd is \"secret\"", "secret", ru->ru_passwd);
	assert_true("PASSWD is NOT enforced", !ENFORCED(ru, RES_USER_PASSWD));

	assert_int_eq("res_user->ru_uid is 45", 45, ru->ru_uid);
	assert_true("UID is NOT enforced", !ENFORCED(ru, RES_USER_UID));

	assert_int_eq("res_user->ru_gid is 90", 90, ru->ru_gid);
	assert_true("GID is NOT enforced", !ENFORCED(ru, RES_USER_GID));

	assert_str_eq("res_user->ru_gecos is \"GECOS\"", "GECOS", ru->ru_gecos);
	assert_true("GECOS is NOT enforced", !ENFORCED(ru, RES_USER_GECOS));

	assert_str_eq("res_user->ru_dir is \"/home/user\"", "/home/user", ru->ru_dir);
	assert_true("DIR is NOT enforced", !ENFORCED(ru, RES_USER_DIR));

	assert_str_eq("res_user->ru_shell is \"/bin/bash\"", "/bin/bash", ru->ru_shell);
	assert_true("SHELL is NOT enforced", !ENFORCED(ru, RES_USER_SHELL));

	assert_int_eq("res_user->ru_mkhome is 1", 1, ru->ru_mkhome);
	assert_str_eq("res_user->ru_skel is \"/etc/skel.oper\"", "/etc/skel.oper", ru->ru_skel);
	assert_true("MKHOME is NOT enforced", !ENFORCED(ru, RES_USER_MKHOME));

	assert_int_eq("res_user->ru_lock is 0", 0, ru->ru_lock);
	assert_true("LOCK is enforced", ENFORCED(ru, RES_USER_LOCK));

	assert_int_eq("res_user->ru_pwmin is 4", 4, ru->ru_pwmin);
	assert_true("PWMIN is NOT enforced", !ENFORCED(ru, RES_USER_PWMIN));

	assert_int_eq("res_user->ru_pwmax is 50", 50, ru->ru_pwmax);
	assert_true("PWMAX is NOT enforced", !ENFORCED(ru, RES_USER_PWMAX));

	assert_int_eq("res_user->ru_pwwarn is 7", 7, ru->ru_pwwarn);
	assert_true("PWWARN is NOT enforced", !ENFORCED(ru, RES_USER_PWWARN));

	assert_int_eq("res_user->ru_inact is 6000", 6000, ru->ru_inact);
	assert_true("INACT is NOT enforced", !ENFORCED(ru, RES_USER_INACT));

	assert_int_eq("res_user->ru_expire is 9000", 9000, ru->ru_expire);
	assert_true("EXPIRE is enforced", ENFORCED(ru, RES_USER_EXPIRE));

	res_user_free(ru);
}

void test_res_user_attrs()
{
	struct res_user *ru;
	struct hash *h;

	test("RES_USER: attribute retrieval");
	h = hash_new();
	ru = res_user_new("user");
	assert_int_eq("retrieved res_user attrs", 0, res_user_attrs(ru, h));
	assert_attr_hash("ru", h, "username", "user");
	assert_attr_hash("ru", h, "present",  "yes"); // default
	assert_null("h[uid] is NULL (unset)", hash_get(h, "uid"));
	assert_null("h[gid] is NULL (unset)", hash_get(h, "gid"));
	assert_null("h[home] is NULL (unset)", hash_get(h, "home"));
	assert_null("h[locked] is NULL (unset)", hash_get(h, "locked"));
	assert_null("h[comment] is NULL (unset)", hash_get(h, "comment"));
	assert_null("h[shell] is NULL (unset)", hash_get(h, "shell"));
	assert_null("h[password] is NULL (unset)", hash_get(h, "password"));
	assert_null("h[pwmin] is NULL (unset)", hash_get(h, "pwmin"));
	assert_null("h[pwmax] is NULL (unset)", hash_get(h, "pwmax"));
	assert_null("h[pwwarn] is NULL (unset)", hash_get(h, "pwwarn"));
	assert_null("h[inact] is NULL (unset)", hash_get(h, "inact"));
	assert_null("h[expiration] is NULL (unset)", hash_get(h, "expiration"));
	assert_null("h[skeleton] is NULL (unset)", hash_get(h, "skeleton"));

	res_user_set(ru, "uid",        "1001");
	res_user_set(ru, "gid",        "2002");
	res_user_set(ru, "home",       "/home/user");
	res_user_set(ru, "locked",     "yes");
	res_user_set(ru, "comment",    "User");
	res_user_set(ru, "shell",      "/bin/bash");
	res_user_set(ru, "password",   "secret");
	res_user_set(ru, "pwmin",      "2");
	res_user_set(ru, "pwmax",      "30");
	res_user_set(ru, "pwwarn",     "7");
	res_user_set(ru, "inact",      "14");
	res_user_set(ru, "expiration", "365");
	res_user_set(ru, "skeleton",   "/etc/skel");
	res_user_set(ru, "present",    "no");
	assert_int_eq("retrieved res_user attrs", 0, res_user_attrs(ru, h));
	assert_attr_hash("ru", h, "uid",        "1001");
	assert_attr_hash("ru", h, "gid",        "2002");
	assert_attr_hash("ru", h, "home",       "/home/user");
	assert_attr_hash("ru", h, "locked",     "yes");
	assert_attr_hash("ru", h, "comment",    "User");
	assert_attr_hash("ru", h, "shell",      "/bin/bash");
	assert_attr_hash("ru", h, "password",   "secret");
	assert_attr_hash("ru", h, "pwmin",      "2");
	assert_attr_hash("ru", h, "pwmax",      "30");
	assert_attr_hash("ru", h, "pwwarn",     "7");
	assert_attr_hash("ru", h, "inact",      "14");
	assert_attr_hash("ru", h, "expiration", "365");
	assert_attr_hash("ru", h, "skeleton",   "/etc/skel");
	assert_attr_hash("ru", h, "present",    "no");

	res_user_set(ru, "locked", "no");
	assert_int_eq("retrieved res_user attrs", 0, res_user_attrs(ru, h));
	assert_attr_hash("ru", h, "locked", "no");

	hash_free(h);
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

	env.group_grdb = grdb_init(DATAROOT "/group");
	if (!env.group_grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	env.group_sgdb = sgdb_init(DATAROOT "/gshadow");
	if (!env.group_sgdb) {
		assert_fail("Unable to init gshadow");
		return;
	}

	test("RES_GROUP: Diffstat");
	assert_int_eq("res_group_stat returns 0", res_group_stat(rg, &env), 0);

	assert_true("NAME is in compliance", !DIFFERENT(rg, RES_GROUP_NAME));
	assert_true("GID is not in compliance", DIFFERENT(rg, RES_GROUP_GID));
	assert_true("MEMBERS is not in compliance", DIFFERENT(rg, RES_GROUP_MEMBERS));
	assert_true("ADMINS is not in compliance", DIFFERENT(rg, RES_GROUP_ADMINS));

	test("RES_GROUP: Fixups (existing account)");
	report = res_group_fixup(rg, 0, &env);
	assert_not_null("res_group_fixup returns a report", report);
	assert_int_eq("group is fixed", report->fixed, 1);
	assert_int_eq("group is now compliant", report->compliant, 1);

	assert_str_eq("rg_name is still set properly", rg->rg_grp->gr_name, "service");

	list = stringlist_new(rg->rg_grp->gr_mem);
	assert_stringlist(list, "post-fixup gr_mem", 2, "account2", "account3");
	stringlist_free(list);

	assert_str_eq("sg_namp is still set properly", rg->rg_sg->sg_namp, "service");
	assert_int_eq("rg_gid is updated properly", rg->rg_grp->gr_gid, 6000);

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

	env.group_grdb = grdb_init(DATAROOT "/group");
	if (!env.group_grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	env.group_sgdb = sgdb_init(DATAROOT "/gshadow");
	if (!env.group_sgdb) {
		assert_fail("Unable to init gshadow");
		grdb_free(env.group_grdb);
		return;
	}

	test("RES_GROUP: Fixups (new account)");
	assert_int_eq("res_group_stat returns 0", res_group_stat(rg, &env), 0);
	report = res_group_fixup(rg, 0, &env);
	assert_not_null("res_group_fixup returns a report", report);
	assert_int_eq("group is fixed", report->fixed, 1);
	assert_int_eq("group is now compliant", report->compliant, 1);

	assert_str_eq("rg_name is still set properly", rg->rg_grp->gr_name, "new_group");

	assert_str_eq("sg_namp is still set properly", rg->rg_sg->sg_namp, "new_group");
	assert_int_eq("rg_gid is updated properly", rg->rg_grp->gr_gid, 6010);

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

	env.group_grdb = grdb_init(DATAROOT "/group");
	if (!env.group_grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	env.group_sgdb = sgdb_init(DATAROOT "/gshadow");
	if (!env.group_sgdb) {
		assert_fail("Unable to init gshadow");
		grdb_free(env.group_grdb);
		return;
	}

	test("RES_GROUP: Fixups (remove existing account)");
	assert_int_eq("res_group_stat returns 0", res_group_stat(rg, &env), 0);
	assert_not_null("(test sanity) group found in group file",   rg->rg_grp);
	assert_not_null("(test sanity) group found in gshadow file", rg->rg_sg);

	report = res_group_fixup(rg, 0, &env);
	assert_not_null("res_group_fixup returns a report", report);
	assert_int_eq("group is fixed", report->fixed, 1);
	assert_int_eq("group is now compliant", report->compliant, 1);

	assert_int_eq("grdb_write succeeds", 0, grdb_write(env.group_grdb, TMPROOT "/group.new"));
	assert_int_eq("sgdb_write succeeds", 0, sgdb_write(env.group_sgdb, TMPROOT "/gshadow.new"));

	env_after.group_grdb = grdb_init(TMPROOT "/group.new");
	if (!env_after.group_grdb) {
		assert_fail("Unable to init grdb_after");
		return;
	}

	env_after.group_sgdb = sgdb_init(TMPROOT "/gshadow.new");
	if (!env_after.group_sgdb) {
		assert_fail("Unable to init sgdb_after");
		grdb_free(env_after.group_grdb);
		return;
	}

	res_group_free(rg);
	rg = res_group_new("non_existent_group");

	assert_int_eq("res_group_stat returns zero (after)", res_group_stat(rg, &env_after), 0);
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

	env.group_grdb = grdb_init(DATAROOT "/group");
	if (!env.group_grdb) {
		assert_fail("Unable to init grdb");
		return;
	}

	env.group_sgdb = sgdb_init(DATAROOT "/gshadow");
	if (!env.group_sgdb) {
		assert_fail("Unable to init gshadow");
		grdb_free(env.group_grdb);
		return;
	}

	test("RES_GROUP: Fixups (remove nonexistent account)");
	assert_int_eq("res_group_stat returns 0", res_group_stat(rg, &env), 0);
	assert_null("(test sanity) group not found in group file",   rg->rg_grp);
	assert_null("(test sanity) group not found in gshadow file", rg->rg_sg);

	report = res_group_fixup(rg, 0, &env);
	assert_not_null("res_group_fixup returns a report", report);
	assert_int_eq("group was already compliant", report->fixed, 0);
	assert_int_eq("group is now compliant", report->compliant, 1);

	assert_int_eq("grdb_write succeeds", 0, grdb_write(env.group_grdb, TMPROOT "/group.new"));
	assert_int_eq("sgdb_write succeeds", 0, sgdb_write(env.group_sgdb, TMPROOT "/gshadow.new"));

	env_after.group_grdb = grdb_init(TMPROOT "/group.new");
	if (!env_after.group_grdb) {
		assert_fail("Unable to init grdb_after");
		return;
	}

	env_after.group_sgdb = sgdb_init(TMPROOT "/gshadow.new");
	if (!env_after.group_sgdb) {
		assert_fail("Unable to init sgdb_after");
		grdb_free(env_after.group_grdb);
		return;
	}

	res_group_free(rg);
	rg = res_group_new("non_existent_group");

	assert_int_eq("res_group_stat returns zero (after)", res_group_stat(rg, &env_after), 0);
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
	assert_int_eq("group2 matches name=group2", 0, res_group_match(rg, "name", "group2"));
	assert_int_eq("group2 matches gid=1337", 0, res_group_match(rg, "gid", "1337"));

	assert_int_ne("group2 does not match name=wheel", 0, res_group_match(rg, "name", "wheel"));
	assert_int_ne("group2 does not match gid=6", 0, res_group_match(rg, "gid", "6"));

	assert_int_ne("password is not a matchable attr", 0, res_group_match(rg, "password", "sesame"));

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
	assert_str_eq("packs properly (normal case)", expected, packed);

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
	assert_str_eq("res_group->key is \"groupkey\"", "groupkey", rg->key);
	assert_str_eq("res_group->rg_name is \"staff\"", "staff", rg->rg_name);
	assert_true("'name' is enforced", ENFORCED(rg, RES_GROUP_NAME));

	assert_str_eq("res_group->rg_passwd is \"sesame\"", "sesame", rg->rg_passwd);
	assert_true("'password' is NOT enforced", !ENFORCED(rg, RES_GROUP_PASSWD));

	assert_int_eq("res_group->rg_gid is 1415", 1415, rg->rg_gid); // 1415(10) == 0587(16)
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

void test_res_group_attrs()
{
	struct res_group *rg;
	struct hash *h;

	h = hash_new();
	test("RES_GROUP: attribute retrieval");
	rg = res_group_new("gr1");
	assert_int_eq("retrieved res_group attrs", 0, res_group_attrs(rg, h));
	assert_attr_hash("rg", h, "name", "gr1");
	assert_attr_hash("rg", h, "present", "yes");
	assert_null("h[gid] is NULL (unset)", hash_get(h, "gid"));
	assert_null("h[password] is NULL (unset)", hash_get(h, "password"));
	assert_null("h[members] is NULL (unset)", hash_get(h, "members"));
	assert_null("h[admins] is NULL (unset)", hash_get(h, "admins"));

	res_group_set(rg, "gid", "707");
	res_group_set(rg, "password", "secret");
	assert_int_eq("retrieved res_group attrs", 0, res_group_attrs(rg, h));
	assert_attr_hash("rg", h, "name", "gr1");
	assert_attr_hash("rg", h, "gid", "707");
	assert_attr_hash("rg", h, "password", "secret");

	res_group_set(rg, "member", "u1");
	res_group_set(rg, "member", "!bad");
	res_group_set(rg, "admin", "a1");
	res_group_set(rg, "admin", "!bad");
	res_group_set(rg, "present", "no");
	assert_int_eq("retrieved res_group attrs", 0, res_group_attrs(rg, h));
	assert_attr_hash("rg", h, "members", "u1 !bad");
	assert_attr_hash("rg", h, "admins",  "a1 !bad");
	assert_attr_hash("rg", h, "present", "no");

	assert_int_ne("xyzzy is not a valid attribute", 0, res_group_set(rg, "xyzzy", "BAD"));
	assert_int_eq("retrieved attrs", 0, res_group_attrs(rg, h));
	assert_null("h[xyzzy] is NULL (bad attr)", hash_get(h, "xyzzy"));

	res_group_free(rg);
	hash_free(h);
}

void test_res_group_attrs_multivalue()
{
	struct res_group *add_only;
	struct res_group *rm_only;
	struct res_group *no_change;
	struct hash *h;

	h = hash_new();
	test("RES_GROUP: Multi-value attribute hash retrieval");
	add_only = res_group_new("gr1");
	res_group_set(add_only, "member", "joe");
	res_group_set(add_only, "member", "bob");
	res_group_set(add_only, "admin",  "alice");
	res_group_set(add_only, "admin",  "zelda");
	assert_int_eq("retrieved res_group attrs", 0, res_group_attrs(add_only, h));
	assert_attr_hash("add_only", h, "members", "joe bob");
	assert_attr_hash("add_only", h, "admins",  "alice zelda");
	res_group_free(add_only);
	hash_free(h);

	h = hash_new();
	rm_only = res_group_new("gr2");
	res_group_set(rm_only, "member", "!joe");
	res_group_set(rm_only, "member", "!bob");
	res_group_set(rm_only, "admin",  "!alice");
	res_group_set(rm_only, "admin",  "!zelda");
	assert_int_eq("retrieved res_group attrs", 0, res_group_attrs(rm_only, h));
	assert_attr_hash("rm_only", h, "members", "!joe !bob");
	assert_attr_hash("rm_only", h, "admins",  "!alice !zelda");
	res_group_free(rm_only);
	hash_free(h);

	h = hash_new();
	no_change = res_group_new("gr3");
	/* whitebox hack testing */
	no_change->enforced |= RES_GROUP_MEMBERS | RES_GROUP_ADMINS;
	assert_int_eq("retrieved res_group attrs", 0, res_group_attrs(no_change, h));
	assert_attr_hash("no_change", h, "members", "");
	assert_attr_hash("no_change", h, "admins",  "");
	res_group_free(no_change);
	hash_free(h);
}

/*****************************************************************************/

void test_res_file_enforcement()
{
	struct res_file *rf;
	rf = res_file_new("sudoers");
	const char *src = DATAROOT "/sha1/file1";

	test("RES_FILE: Default Enforcements");
	assert_true("UID not enforced",  !ENFORCED(rf, RES_FILE_UID));
	assert_true("GID not enforced",  !ENFORCED(rf, RES_FILE_GID));
	assert_true("MODE not enforced", !ENFORCED(rf, RES_FILE_MODE));
	assert_true("SHA1 not enforced", !ENFORCED(rf, RES_FILE_SHA1));

	test("RES_FILE: UID enforcement");
	res_file_set(rf, "owner", "someone");
	assert_true("owner enforced", ENFORCED(rf, RES_FILE_UID));
	assert_str_eq("owner set properly", rf->rf_owner, "someone");

	test("RES_FILE: GID enforcement");
	res_file_set(rf, "group", "somegroup");
	assert_true("group enforced", ENFORCED(rf, RES_FILE_GID));
	assert_str_eq("group set properly", rf->rf_group, "somegroup");

	test("RES_FILE: MODE enforcement");
	res_file_set(rf,"mode", "0755");
	assert_true("mode enforced", ENFORCED(rf, RES_FILE_MODE));
	assert_int_eq("mode set properly", rf->rf_mode, 0755);

	test("RES_FILE: SHA1 / rpath enforcement");
	res_file_set(rf, "source", src);
	assert_true("SHA1 enforced", ENFORCED(rf, RES_FILE_SHA1));
	assert_str_eq("rf_rpath set properly", rf->rf_rpath, src);

	test("RES_FILE: SHA1 / lpath 'enforcement'");
	res_file_set(rf, "path", src);
	assert_str_eq("rf_lpath set properly", rf->rf_lpath, src);

	res_file_free(rf);
}

void test_res_file_diffstat()
{
	struct res_file *rf;

	rf = res_file_new("sudoers");
	res_file_set(rf, "path", DATAROOT "/res_file/sudoers");
	res_file_set(rf, "owner", "someuser");
	rf->rf_uid = 1001;
	res_file_set(rf, "group", "somegroup");
	rf->rf_gid = 2002;
	res_file_set(rf, "mode", "0440");

	test("RES_FILE: res_file_diffstat picks up file differences");
	/* don't need to pass in an env arg (2nd param) */
	assert_int_eq("res_file_stat returns zero", res_file_stat(rf, NULL), 0);
	assert_int_eq("File exists", 1, rf->rf_exists);
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

	const char *path = DATAROOT "/res_file/fstab";
	const char *src  = DATAROOT "/res_file/SRC/fstab";

	test("RES_FILE: File Remediation");

	/* STAT the target file */
	if (stat(path, &st) != 0) {
		assert_fail("RES_FILE: Unable to stat pre-remediation file");
		return;
	}
	assert_int_ne("Pre-remediation: file owner UID is not 65542", st.st_uid, 65542);
	assert_int_ne("Pre-remediation: file group GID is not 65524", st.st_gid, 65524);
	assert_int_ne("Pre-remediation: file permissions are not 0754", st.st_mode & 07777, 0754);

	rf = res_file_new("fstab");
	res_file_set(rf, "path",  DATAROOT "/res_file/fstab");
	res_file_set(rf, "owner", "someuser");
	rf->rf_uid = 65542;
	res_file_set(rf, "group", "somegroup");
	rf->rf_gid = 65524;
	res_file_set(rf, "mode",  "0754");
	res_file_set(rf, "source", src);

	/* set up the resource_env for content remediation */
	env.file_fd = open(src, O_RDONLY);
	assert_int_gt("(test sanity) able to open 'remote' file", env.file_fd, 0);
	env.file_len = st.st_size;

	assert_int_eq("res_file_stat succeeds", res_file_stat(rf, &env), 0);
	assert_int_eq("File exists", 1, rf->rf_exists);
	assert_true("UID is out of compliance",  DIFFERENT(rf, RES_FILE_UID));
	assert_true("GID is out of compliance",  DIFFERENT(rf, RES_FILE_GID));
	report = res_file_fixup(rf, 0, &env);
	assert_not_null("res_file_fixup returns a report", report);
	assert_int_eq("file was fixed", report->fixed, 1);
	assert_int_eq("file is now compliant", report->compliant, 1);

	/* STAT the fixed up file */
	if (stat(path, &st) != 0) {
		assert_fail("RES_FILE: Unable to stat post-remediation file");
		return;
	}
	assert_int_eq("Post-remediation: file owner UID 65542", st.st_uid, 65542);
	assert_int_eq("Post-remediation: file group GID 65524", st.st_gid, 65524);
	assert_int_eq("Post-remediation: file permissions are 0754", st.st_mode & 07777, 0754);

	res_file_free(rf);
	report_free(report);
}

void test_res_file_fixup_new()
{
	struct stat st;
	struct res_file *rf;
	struct report *report;
	struct resource_env env;

	const char *path = DATAROOT "/res_file/new_file";

	env.file_fd = -1;
	env.file_len = 0;

	test("RES_FILE: File Remediation (new file)");
	rf = res_file_new(path);
	res_file_set(rf, "owner", "someuser");
	rf->rf_uid = 65542;
	res_file_set(rf, "group", "somegroup");
	rf->rf_gid = 65524;
	res_file_set(rf, "mode",  "0754");

	assert_int_eq("res_file_stat succeeds", res_file_stat(rf, &env), 0);
	assert_int_eq("File does not already exist", 0, rf->rf_exists);
	assert_int_ne("stat pre-remediation file returns non-zero", 0, stat(path, &st));

	report = res_file_fixup(rf, 0, &env);
	assert_not_null("res_file_fixup returns a report", report);
	assert_int_eq("file is fixed", report->fixed, 1);
	assert_int_eq("file is now compliant", report->compliant, 1);
	assert_int_eq("stat post-remediation file returns zero", 0, stat(path, &st));

	/* STAT the fixed up file */
	if (stat(path, &st) != 0) {
		assert_fail("RES_FILE: Unable to stat post-remediation file");
		return;
	}
	assert_int_eq("Post-remediation: file owner UID 65542", st.st_uid, 65542);
	assert_int_eq("Post-remediation: file group GID 65524", st.st_gid, 65524);
	assert_int_eq("Post-remediation: file permissions are 0754", st.st_mode & 07777, 0754);

	res_file_free(rf);
	report_free(report);
}

void test_res_file_fixup_remove_existing()
{
	struct stat st;
	struct res_file *rf;
	struct report *report;
	struct resource_env env;

	const char *path = DATAROOT "/res_file/delete";

	env.file_fd = -1;
	env.file_len = 0;

	test("RES_FILE: File Remediation (remove existing file)");
	rf = res_file_new(path);
	res_file_set(rf, "present", "no"); /* Remove the file */

	assert_int_eq("res_file_stat succeeds", res_file_stat(rf, &env), 0);
	assert_int_eq("File exists", 1, rf->rf_exists);
	assert_int_eq("stat pre-remediation file returns zero", 0, stat(path, &st));

	report = res_file_fixup(rf, 0, &env);
	assert_not_null("res_file_fixup returns a report", report);
	assert_int_eq("file is fixed", report->fixed, 1);
	assert_int_eq("file is now compliant", report->compliant, 1);
	assert_int_ne("stat post-remediation file returns non-zero", 0, stat(path, &st));

	res_file_free(rf);
	report_free(report);
}

void test_res_file_fixup_remove_nonexistent()
{
	struct stat st;
	struct res_file *rf;
	struct report *report;
	struct resource_env env;

	const char *path = DATAROOT "/res_file/non-existent";

	env.file_fd = -1;
	env.file_len = 0;

	test("RES_FILE: File Remediation (remove non-existent file)");
	rf = res_file_new(path);
	res_file_set(rf, "present", "no"); /* Remove the file */

	assert_int_eq("res_file_stat succeeds", res_file_stat(rf, &env), 0);
	assert_int_eq("File does not already exist", 0, rf->rf_exists);
	assert_int_ne("stat pre-remediation file returns non-zero", 0, stat(path, &st));

	report = res_file_fixup(rf, 0, &env);
	assert_not_null("res_file_fixup returns a report", report);
	assert_int_eq("file was already compliant", report->fixed, 0);
	assert_int_eq("file is now compliant", report->compliant, 1);
	assert_int_ne("stat post-remediation file returns non-zero", 0, stat(path, &st));

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
	assert_int_eq("SUDO matches path=/etc/sudoers", 0, res_file_match(rf, "path", "/etc/sudoers"));

	assert_int_ne("SUDO does not match path=/tmp/wrong/file", 0, res_file_match(rf, "path", "/tmp/wrong/file"));

	assert_int_ne("owner is not a matchable attr", 0, res_file_match(rf, "owner", "someuser"));

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
	assert_str_eq("packs properly (normal case)", expected, packed);

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
	assert_str_eq("res_file->key is \"somefile\"", "somefile", rf->key);
	assert_str_eq("res_file->rf_lpath is \"/etc/sudoers\"", "/etc/sudoers", rf->rf_lpath);
	assert_str_eq("res_file->rf_rsha1.hex is \"0123456789abcdef0123456789abcdef01234567\"",
	                  "0123456789abcdef0123456789abcdef01234567", rf->rf_rsha1.hex);
	assert_true("SHA1 is NOT enforced", !ENFORCED(rf, RES_FILE_SHA1));

	assert_str_eq("res_file->rf_owner is \"someuser\"", "someuser", rf->rf_owner);
	assert_true("UID is enforced", ENFORCED(rf, RES_FILE_UID));

	assert_str_eq("Res_file->rf_group is \"somegroup\"", "somegroup", rf->rf_group);
	assert_true("GID is enforced", ENFORCED(rf, RES_FILE_GID));

	assert_int_eq("res_file->rf_mode is 0644", 0644, rf->rf_mode);
	assert_true("MODE is NOT enforced", !ENFORCED(rf, RES_FILE_MODE));

	res_file_free(rf);
}

void test_res_file_attrs()
{
	struct res_file *rf;
	struct hash *h;

	h = hash_new();
	test("RES_FILE: attribute retrieval via hash");
	rf = res_file_new("/etc/sudoers");
	assert_int_eq("attr retrieval worked", 0, res_file_attrs(rf, h));
	assert_attr_hash("rf", h, "path", "/etc/sudoers");
	assert_attr_hash("rf", h, "present", "yes"); // default
	assert_null("h[owner] is NULL (unset)", hash_get(h, "owner"));
	assert_null("h[group] is NULL (unset)", hash_get(h, "group"));
	assert_null("h[mode] is NULL (unset)", hash_get(h, "mode"));
	assert_null("h[template] is NULL (unset)", hash_get(h, "template"));
	assert_null("h[source] is NULL (unset)", hash_get(h, "source"));

	res_file_set(rf, "owner",  "root");
	res_file_set(rf, "group",  "sys");
	res_file_set(rf, "mode",   "0644");
	res_file_set(rf, "source", "/srv/files/sudo");
	assert_int_eq("attr retrieval worked", 0, res_file_attrs(rf, h));
	assert_attr_hash("rf", h, "owner",  "root");
	assert_attr_hash("rf", h, "group",  "sys");
	assert_attr_hash("rf", h, "mode",   "0644");
	assert_attr_hash("rf", h, "source", "/srv/files/sudo");
	assert_null("h[template] is NULL (unset)", hash_get(h, "template"));

	res_file_set(rf, "template", "/srv/tpl/sudo");
	res_file_set(rf, "present", "no");
	assert_int_eq("attr retrieval worked", 0, res_file_attrs(rf, h));
	assert_attr_hash("rf", h, "template", "/srv/tpl/sudo");
	assert_attr_hash("rf", h, "present",  "no");
	assert_null("h[source] is NULL (unset)", hash_get(h, "source"));

	assert_int_ne("xyzzy is not a valid attribute", 0, res_file_set(rf, "xyzzy", "BAD"));
	assert_int_eq("retrieved attrs", 0, res_file_attrs(rf, h));
	assert_null("h[xyzzy] is NULL (bad attr)", hash_get(h, "xyzzy"));

	hash_free(h);
	res_file_free(rf);
}

/*****************************************************************************/

#define PACKAGE "xbill"
void test_res_package_diffstat_fixup()
{
	struct res_package *r;
	struct resource_env env;
	struct report *report;
	char *version;

	env.package_manager = DEFAULT_PM; /* from test/local.h */

	test("RES_PACKAGE: stat (package needs removed)");
	/* first, ensure that the package is not installed */
	assert_int_eq("(test sanity) removal of package succeeds",
		package_remove(DEFAULT_PM, PACKAGE), 0);

	/* then, make sure we can install it. */
	assert_int_eq("(test sanity) installation of package succeeds",
		package_install(DEFAULT_PM, PACKAGE, NULL), 0);

	/* now the real parts of the test */
	r = res_package_new(PACKAGE);
	res_package_set(r, "installed", "no");

	assert_int_eq("res_package_stat succeeds", res_package_stat(r, &env), 0);
	assert_not_null("package '" PACKAGE "' is installed", r->installed);
	version = strdup(r->installed); // save version for later

	test("RES_PACKAGE: fixup (package needs removed)");
	report = res_package_fixup(r, 0, &env);
	assert_not_null("res_package_fixup returns a report", report);
	assert_true("resource was fixed", report->fixed);
	assert_true("resource is compliant", report->compliant);
	report_free(report);

	/* check it */
	assert_null("Package is uninstalled now", package_version(DEFAULT_PM, PACKAGE));

	test("RES_PACKAGE: stat (package needs installed)");
	res_package_set(r, "installed", "yes");
	res_package_set(r, "version",   version);
	assert_int_eq("res_package_stat succeeds", res_package_stat(r, &env), 0);
	assert_null("package '" PACKAGE "' is NOT installed", r->installed);

	test("RES_PACKAGE: fixup (package needs installed)");
	report = res_package_fixup(r, 0, &env);
	assert_not_null("res_package_fixup returns a report (2)", report);
	assert_true("resource was fixed", report->fixed);
	assert_true("resource is compliant", report->compliant);
	report_free(report);

	/* check it */
	free(version);
	version = package_version(DEFAULT_PM, PACKAGE);
	assert_not_null("Package is installed now", version);
	assert_str_eq("Correct version is installed", version, r->version);

	free(version);
}

void test_res_package_match()
{
	struct res_package *rp;

	rp = res_package_new("sys-tools");

	res_package_set(rp, "name", "lsof");
	res_package_set(rp, "installed", "yes");

	test("RES_PACKAGE: attribute matching");
	assert_int_eq("sys-tools matches name=lsof", 0, res_package_match(rp, "name", "lsof"));
	assert_int_ne("sys-tools does not match name=dtrace", 0, res_package_match(rp, "name", "dtrace"));

	assert_int_ne("installed is not a matchable attr", 0, res_package_match(rp, "installed", "yes"));

	res_package_free(rp);
}

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
	assert_str_eq("res_package->key is \"pkgkey\"", "pkgkey", r->key);
	assert_str_eq("res_package->name is \"libtao-dev\"", "libtao-dev", r->name);
	assert_str_eq("res_package->version is \"5.6.3\"", "5.6.3", r->version);
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

void test_res_package_attrs()
{
	struct res_package *r;
	struct hash *h;

	test("RES_PACKAGE: attribute retrieval");
	h = hash_new();
	r = res_package_new("pkg");
	assert_int_eq("retrieved attrs", 0, res_package_attrs(r, h));
	assert_attr_hash("r", h, "name", "pkg");
	assert_attr_hash("r", h, "installed", "yes"); // default
	assert_null("h[version] is NULL (not set)", hash_get(h, "version"));

	res_package_set(r, "name", "extra-tools");
	res_package_set(r, "version", "1.2.3-4.5.6");
	res_package_set(r, "installed", "no");
	assert_int_eq("retrieved attrs", 0, res_package_attrs(r, h));
	assert_attr_hash("r", h, "name", "extra-tools");
	assert_attr_hash("r", h, "version", "1.2.3-4.5.6");
	assert_attr_hash("r", h, "installed", "no");

	assert_int_ne("xyzzy is not a valid attribute", 0, res_package_set(r, "xyzzy", "BAD"));
	assert_int_eq("retrieved attrs", 0, res_package_attrs(r, h));
	assert_null("h[xyzzy] is NULL (bad attr)", hash_get(h, "xyzzy"));

	hash_free(h);
	res_package_free(r);
}

/*****************************************************************************/

#define SERVICE "snmpd"
void test_res_service_diffstat_fixup()
{
	struct res_service *r;
	struct resource_env env;
	struct report *report;

	/* so we can clean up after ourselves */
	int was_enabled = 0;
	int was_running = 0;

	env.service_manager = DEFAULT_SM; /* from test/local.h */

	test("RES_SERVICE: stat (service needs enabled / started)");
	/* first make sure the service is stopped and disabled */
	if (service_enabled(DEFAULT_SM, SERVICE) == 0) {
		service_disable(DEFAULT_SM, SERVICE);
		was_enabled = 1;
	}
	assert_int_ne("Service '" SERVICE "' disabled",
		service_enabled(DEFAULT_SM, SERVICE), 0);
	if (service_running(DEFAULT_SM, SERVICE) == 0) {
		service_stop(DEFAULT_SM, SERVICE);
		was_running = 1;
	}
	assert_int_ne("Service '" SERVICE "' not running",
		service_running(DEFAULT_SM, SERVICE), 0);

	/* now the real test */
	r = res_service_new("snmpd");
	res_service_set(r, "enabled", "yes");
	res_service_set(r, "running", "yes");

	assert_int_eq("res_service_stat succeeds", res_service_stat(r, &env), 0);
	assert_false("service not running (non-compliant)", r->running);
	assert_true("Service should be running",      ENFORCED(r, RES_SERVICE_RUNNING));
	assert_true("Service should be running (2)", !ENFORCED(r, RES_SERVICE_STOPPED));
	assert_false("service not enabled (non-compliant)", r->enabled);
	assert_true("Service should be enabled",      ENFORCED(r, RES_SERVICE_ENABLED));
	assert_true("Service should be enabled (2)", !ENFORCED(r, RES_SERVICE_DISABLED));

	test("RES_SERVICE: fixup (service needs enabled / started)");
	report = res_service_fixup(r, 0, &env);
	assert_not_null("res_service_fixup returns a report", report);
	assert_true("resource was fixed", report->fixed);
	assert_true("resource is compliant", report->compliant);
	report_free(report);
	sleep(1);

	assert_int_eq("Service '" SERVICE "' is now running",
		service_running(DEFAULT_SM, SERVICE), 0);
	assert_int_eq("Service '" SERVICE "' is now enabled",
		service_enabled(DEFAULT_SM, SERVICE), 0);

	test("RES_SERVICE: stat (service needs disabled / stopped)");
	res_service_set(r, "enabled", "no");
	res_service_set(r, "running", "no");

	assert_int_eq("res_service_stat succeeds (2)", res_service_stat(r, &env), 0);
	assert_true("service running (non-compliant)", r->running);
	assert_true("Service should be stopped",      ENFORCED(r, RES_SERVICE_STOPPED));
	assert_true("Service should be stopped (2)", !ENFORCED(r, RES_SERVICE_RUNNING));
	assert_true("service enabled (non-compliant)", r->enabled);
	assert_true("Service should be disabled",      ENFORCED(r, RES_SERVICE_DISABLED));
	assert_true("Service should be disabled (2)", !ENFORCED(r, RES_SERVICE_ENABLED));

	test("RES_SERVICE: fixup (service needs disabled / stopped)");
	report = res_service_fixup(r, 0, &env);
	assert_not_null("res_service_fixup returns a report (2)", report);
	assert_true("resource was fixed", report->fixed);
	assert_true("resource is compliant", report->compliant);
	report_free(report);
	sleep(1);

	assert_int_ne("Service '" SERVICE "' is now stopped",
		service_running(DEFAULT_SM, SERVICE), 0);
	assert_int_ne("Service '" SERVICE "' is now disabled",
		service_enabled(DEFAULT_SM, SERVICE), 0);

	/* clean up after ourselves */
	if (was_enabled) {
		service_enable(DEFAULT_SM, SERVICE);
		sleep(1);
		assert_int_eq("Cleanup: service re-enabled",
			service_enabled(DEFAULT_SM, SERVICE), 0);
	} else {
		service_disable(DEFAULT_SM, SERVICE);
		sleep(1);
		assert_int_ne("Cleanup: service re-disabled",
			service_enabled(DEFAULT_SM, SERVICE), 0);
	}

	if (was_running) {
		service_start(DEFAULT_SM, SERVICE);
		sleep(1);
		assert_int_eq("Cleanup: service re-started",
			service_running(DEFAULT_SM, SERVICE), 0);
	} else {
		service_stop(DEFAULT_SM, SERVICE);
		sleep(1);
		assert_int_ne("Cleanup: service stopped",
			service_running(DEFAULT_SM, SERVICE), 0);
	}
}

void test_res_service_match()
{
	struct res_service *rs;

	rs = res_service_new("svc1");

	res_service_set(rs, "service", "policyd");
	res_service_set(rs, "running", "yes");

	test("RES_SERVICE: attribute matching");
	assert_int_eq("svc1 matches service=policyd", 0, res_service_match(rs, "service", "policyd"));
	assert_int_ne("svc1 does not match service=apache2", 0, res_service_match(rs, "service", "apache2"));

	assert_int_ne("running is not a matchable attr", 0, res_service_match(rs, "running", "yes"));

	res_service_free(rs);
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

	assert_str_eq("packs properly (normal case)", expected, packed);
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
	assert_str_eq("res_service->key is \"svckey\"", "svckey", r->key);
	assert_str_eq("res_service->service is \"service-name\"", "service-name", r->service);
	assert_true("RUNNING is enforced", ENFORCED(r, RES_SERVICE_RUNNING));
	assert_false("STOPPED is NOT enforced", ENFORCED(r, RES_SERVICE_STOPPED));
	assert_true("ENABLED is enforced", ENFORCED(r, RES_SERVICE_ENABLED));
	assert_false("DISABLED is NOT enforced", ENFORCED(r, RES_SERVICE_DISABLED));

	res_service_free(r);
}

void test_res_service_attrs()
{
	struct res_service *r;
	struct hash *h;

	h = hash_new();
	test("RES_SERVICE: attribute retrieval");
	r = res_service_new("svc");
	assert_int_eq("retrieved attrs", 0, res_service_attrs(r, h));
	assert_attr_hash("r", h, "name", "svc");
	assert_attr_hash("r", h, "running", "no"); // default
	assert_attr_hash("r", h, "enabled", "no"); // default

	res_service_set(r, "running", "yes");
	res_service_set(r, "enabled", "yes");
	assert_int_eq("retrieved attrs", 0, res_service_attrs(r, h));
	assert_attr_hash("r", h, "running", "yes");
	assert_attr_hash("r", h, "enabled", "yes");

	res_service_set(r, "service", "httpd");
	res_service_set(r, "running", "no");
	res_service_set(r, "enabled", "no");
	assert_int_eq("retrieved attrs", 0, res_service_attrs(r, h));
	assert_attr_hash("r", h, "name", "httpd");
	assert_attr_hash("r", h, "running", "no");
	assert_attr_hash("r", h, "enabled", "no");

	res_service_set(r, "stopped", "no");
	res_service_set(r, "disabled", "no");
	assert_int_eq("retrieved attrs", 0, res_service_attrs(r, h));
	assert_attr_hash("r", h, "running", "yes");
	assert_attr_hash("r", h, "enabled", "yes");

	res_service_set(r, "stopped", "yes");
	res_service_set(r, "disabled", "yes");
	assert_int_eq("retrieved attrs", 0, res_service_attrs(r, h));
	assert_attr_hash("r", h, "running", "no");
	assert_attr_hash("r", h, "enabled", "no");

	assert_int_ne("xyzzy is not a valid attribute", 0, res_service_set(r, "xyzzy", "BAD"));
	assert_int_eq("retrieved attrs", 0, res_service_attrs(r, h));
	assert_null("h[xyzzy] is NULL (bad attr)", hash_get(h, "xyzzy"));

	hash_free(h);
	res_service_free(r);
}

void test_res_service_notify()
{
	struct res_service *r;

	test("RES_SERVICE: notify");
	r = res_service_new("svc");
	assert_int_ne("By default, service hasn't been notified", r->notified, 1);
	assert_int_eq("res_service_notify returns 0", res_service_notify(r, NULL), 0);
	assert_int_eq("Service has now been notified", r->notified, 1);

	res_service_free(r);
}

/*****************************************************************************/

void test_res_host_diffstat_fixup()
{
	struct res_host *r;
	struct resource_env env;
	struct report *report;

	env.aug_context = augcw_init();

	test("RES_HOST: stat / fixup (initial)");
	r = res_host_new("dne.niftylogic.net");
	res_host_set(r, "ip", "192.168.224.1");
	res_host_set(r, "alias", "bad.cw.niftylogic.net");

	assert_int_eq("res_host_stat succeeds", res_host_stat(r, &env), 0);
	report = res_host_fixup(r, 0, &env);
	assert_not_null("res_host_fixup returns a report", report);
	report_free(report);

	test("RES_HOST: stat / fixup (host needs removed)");
	res_host_set(r, "present", "no");

	assert_int_eq("res_host_stat succeeds", res_host_stat(r, &env), 0);
	assert_not_null("r->aug_root is NOT NULL (found a match)", r->aug_root);
	report = res_host_fixup(r, 0, &env);
	assert_not_null("res_host_fixup returns a report", report);
	assert_true("resource was fixed", report->fixed);
	assert_true("resource is compliant", report->compliant);
	report_free(report);

	test("RES_HOST: stat / fixup (host needs added)");
	res_host_set(r, "present", "yes");
	assert_int_eq("res_host_stat succeeds", res_host_stat(r, &env), 0);
	assert_null("r->aug_root is NULL (not found)", r->aug_root);
	report = res_host_fixup(r, 0, &env);
	assert_not_null("res_host_fixup returns a report", report);
	assert_true("resource was fixed", report->fixed);
	assert_true("resource is compliant", report->compliant);
	report_free(report);

	res_host_free(r);
}

void test_res_host_match()
{
	struct res_host *rh;

	rh = res_host_new("host1");
	res_host_set(rh, "ip", "192.168.1.1");
	res_host_set(rh, "alias", "host1.example.net");

	test("RES_HOST: attribute matching");
	assert_int_eq("host1 matches hostname=host1", 0, res_host_match(rh, "hostname", "host1"));
	assert_int_eq("host1 matches ip=192.168.1.1", 0, res_host_match(rh, "ip", "192.168.1.1"));
	assert_int_eq("host1 matches address=192.168.1.1", 0, res_host_match(rh, "address", "192.168.1.1"));

	assert_int_ne("host1 does not match hostname=h2", 0, res_host_match(rh, "hostname", "h2"));
	assert_int_ne("host1 does not match ip=1.1.1.1",  0, res_host_match(rh, "ip", "1.1.1.1"));
	assert_int_ne("host1 does not match address=1.1.1.1",  0, res_host_match(rh, "address", "1.1.1.1"));

	assert_int_ne("alias is not a matchable attr", 0, res_host_match(rh, "alias", "host1.example.net"));

	res_host_free(rh);
}

void test_res_host_pack()
{
	struct res_host *rh;
	char *packed;
	const char *expected;

	test("RES_HOST: pack res_host");
	rh = res_host_new("127");
	res_host_set(rh, "hostname", "localhost");
	res_host_set(rh, "address",  "127.0.0.1");
	res_host_set(rh, "aliases",  "localhost.localdomain box1");

	packed = res_host_pack(rh);
	expected = "res_host::\"127\""
		"00000001" /* RES_HOST_ALIASES only */
		"\"localhost\""
		"\"127.0.0.1\""
		"\"localhost.localdomain box1\"";
	assert_str_eq("packs properly (normal case)", expected, packed);

	free(packed);
	res_host_free(rh);
}

void test_res_host_unpack()
{
	const char *packed;
	struct res_host *rh;

	test("RES_HOST: unpack res_host");
	packed = "res_host::\"127\""
		"00000001" /* RES_HOST_ALIASES only */
		"\"localhost\""
		"\"127.0.0.1\""
		"\"localhost.localdomain box1\"";

	assert_null("res_host_unpack returns NULL on failure", res_host_unpack("<invalid pack data>"));
	rh = res_host_unpack(packed);
	assert_not_null("unpacks successfully (normal case)", rh);

	assert_str_eq("r->key is \"127\"", "127", rh->key);
	assert_str_eq("r->hostname is \"localhost\"", "localhost", rh->hostname);
	assert_str_eq("r->ip is \"127.0.0.1\"", "127.0.0.1", rh->ip);
	assert_stringlist(rh->aliases, "r->aliases", 2, "localhost.localdomain", "box1");

	res_host_free(rh);
}

void test_res_host_attrs()
{
	struct res_host *rh;
	struct hash *h;

	h = hash_new();
	test("RES_HOST: attribute retrieval");
	rh = res_host_new("localhost");
	assert_int_eq("attribute retrieval succeeded", 0, res_host_attrs(rh, h));
	assert_attr_hash("rh", h, "hostname", "localhost");
	assert_attr_hash("rh", h, "present",  "yes"); // default
	assert_null("h[ip] is NULL (unset)", hash_get(h, "ip"));
	assert_null("h[aliases] is NULL (unset)", hash_get(h, "aliases"));

	res_host_set(rh, "ip", "192.168.1.10");
	res_host_set(rh, "alias", "localhost.localdomain");
	res_host_set(rh, "alias", "special");
	assert_int_eq("attribute retrieval succeeded", 0, res_host_attrs(rh, h));
	assert_attr_hash("rh", h, "ip", "192.168.1.10");
	assert_attr_hash("rh", h, "aliases", "localhost.localdomain special");

	res_host_set(rh, "present", "no");
	assert_int_eq("attribute retrieval succeeded", 0, res_host_attrs(rh, h));
	assert_attr_hash("rh", h, "present",  "no");

	hash_free(h);
	res_host_free(rh);
}

/*****************************************************************************/

void test_res_sysctl_diffstat_fixup()
{
	struct res_sysctl *r;
	struct report *report;
	struct resource_env env;

	test("RES_SYSCTL: diffstat");
	r = res_sysctl_new("martians");
	res_sysctl_set(r, "param", "net.ipv4.conf.all.log_martians");
	res_sysctl_set(r, "value", "1");
	res_sysctl_set(r, "persist", "yes");

	/* set up resource env with augeas stuff */
	env.aug_context = augcw_init();
	assert_not_null("Augeas initialized properly", env.aug_context);

	/* test setup ensures that log_martians is 0 in /proc/sys */
	assert_int_eq("res_sysctl_stat succeeds", res_sysctl_stat(r, &env), 0);
	assert_true("VALUE is out of compliance", DIFFERENT(r, RES_SYSCTL_VALUE));

	test("RES_SYSCTL: fixup");
	report = res_sysctl_fixup(r, 0, &env);
	assert_not_null("res_sysctl_fixup returns a report", report);
	assert_int_eq("resource was fixed", report->fixed, 1);
	assert_int_eq("resource is compliant", report->compliant, 1);

	log_set(LOG_LEVEL_DEBUG);
	assert_int_eq("res_systl_stat (after) succeeds", res_sysctl_stat(r, &env), 0);
	assert_true("VALUE is now in compliance", !DIFFERENT(r, RES_SYSCTL_VALUE));
	log_set(LOG_LEVEL_NONE);

	report_free(report);
	res_sysctl_free(r);
}

void test_res_sysctl_match()
{
	struct res_sysctl *rs;

	rs = res_sysctl_new("sys.ctl");
	res_sysctl_set(rs, "value", "0 5 6 1");
	res_sysctl_set(rs, "persist", "yes");

	test("RES_SYSCTL: attribute matching");
	assert_int_eq("sys.ctl matches param=sys.ctl", 0, res_sysctl_match(rs, "param", "sys.ctl"));

	assert_int_ne("sys.ctl does not match param=sys.p", 0, res_sysctl_match(rs, "param", "sys.p"));

	assert_int_ne("value is not a matchable attr",   0, res_sysctl_match(rs, "value", "0 5 6 1"));
	assert_int_ne("persist is not a matchable attr", 0, res_sysctl_match(rs, "persist", "yes"));

	res_sysctl_free(rs);
}

void test_res_sysctl_pack()
{
	struct res_sysctl *rs;
	char *packed;
	const char *expected;

	test("RES_SYSCTL: pack res_sysctl");
	rs = res_sysctl_new("ip");
	res_sysctl_set(rs, "param", "kernel.net.ip");
	res_sysctl_set(rs, "value", "1");
	res_sysctl_set(rs, "persist", "yes");

	packed = res_sysctl_pack(rs);
	expected = "res_sysctl::\"ip\"00000003\"kernel.net.ip\"\"1\"0001";
	assert_str_eq("packs properly (normal case)", expected, packed);

	res_sysctl_free(rs);
	free(packed);
}

void test_res_sysctl_unpack()
{
	struct res_sysctl *rs;
	const char *packed;

	test("RES_SYSCTL: unpack res_sysctl");
	packed = "res_sysctl::\"param\""
		"00000003"
		"\"net.ipv4.echo\""
		"\"2\""
		"0001";

	assert_null("returns NULL on failure", res_sysctl_unpack("<invalid pack data>"));
	rs = res_sysctl_unpack(packed);
	assert_not_null("res_sysctl_unpack succeeds", rs);
	assert_str_eq("res_sysctl->key is \"param\"", "param", rs->key);
	assert_str_eq("res_sysctl->param is \"net.ipv4.echo\"", "net.ipv4.echo", rs->param);

	assert_str_eq("res_sysctl->value is \"2\"", "2", rs->value);
	assert_true("VALUE is enforced", ENFORCED(rs, RES_SYSCTL_VALUE));

	assert_int_eq("res_sysctl->persist is 1", 1, rs->persist);
	assert_true("PERSIST is enforced", ENFORCED(rs, RES_SYSCTL_PERSIST));

	res_sysctl_free(rs);
}

void test_res_sysctl_attrs()
{
	struct res_sysctl *rs;
	struct hash *h;

	h = hash_new();
	test("RES_SYSCTL: attribute retrieval");
	rs = res_sysctl_new("net.ipv4.tun");
	assert_int_eq("retrieved res_sysctl attrs", 0, res_sysctl_attrs(rs, h));
	assert_attr_hash("rs", h, "param", "net.ipv4.tun");
	assert_null("h[value] is NULL (unset)", hash_get(h, "value"));
	assert_attr_hash("rs", h, "persist", "yes");

	res_sysctl_set(rs, "value", "42");
	res_sysctl_set(rs, "persist", "yes");
	assert_int_eq("retrieved res_sysctl attrs", 0, res_sysctl_attrs(rs, h));
	assert_attr_hash("rs", h, "param", "net.ipv4.tun");
	assert_attr_hash("rs", h, "value", "42");
	assert_attr_hash("rs", h, "persist", "yes");

	res_sysctl_set(rs, "persist", "no");
	assert_int_eq("retrieved res_sysctl attrs", 0, res_sysctl_attrs(rs, h));
	assert_attr_hash("rs", h, "persist", "no");

	assert_int_ne("xyzzy is not a valid attribute", 0, res_sysctl_set(rs, "xyzzy", "BAD"));
	assert_int_eq("retrieved attrs", 0, res_sysctl_attrs(rs, h));
	assert_null("h[xyzzy] is NULL (bad attr)", hash_get(h, "xyzzy"));

	hash_free(h);
	res_sysctl_free(rs);
}

/*****************************************************************************/

void test_res_dir_enforcement()
{
	struct res_dir *r;
	r = res_dir_new("/tmp");

	test("RES_DIR: Default Enforcements");
	assert_true("ABSENT not enforced", !ENFORCED(r, RES_DIR_ABSENT));
	assert_true("UID not enforced",    !ENFORCED(r, RES_DIR_UID));
	assert_true("GID not enforced",    !ENFORCED(r, RES_DIR_GID));
	assert_true("MODE not enforced",   !ENFORCED(r, RES_DIR_MODE));

	test("RES_DIR: ABSENT enforcement");
	res_dir_set(r, "present", "no");
	assert_true("ABSENT enforced", ENFORCED(r, RES_DIR_ABSENT));
	res_dir_set(r, "present", "yes");
	assert_true("ABSENT no longer enforced", !ENFORCED(r, RES_DIR_ABSENT));

	test("RES_DIR: UID enforcement");
	res_dir_set(r, "owner", "user1");
	assert_true("UID enforced", ENFORCED(r, RES_DIR_UID));
	assert_str_eq("owner set properly", r->owner, "user1");

	test("RES_DIR: GID enforcement");
	res_dir_set(r, "group", "staff");
	assert_true("GID enforced", ENFORCED(r, RES_DIR_GID));
	assert_str_eq("group set properly", r->group, "staff");

	test("RES_DIR: MODE enforcement");
	res_dir_set(r, "mode", "0750");
	assert_true("MODE enforced", ENFORCED(r, RES_DIR_MODE));
	assert_int_eq("mode set properly", r->mode, 0750);

	res_dir_free(r);
}

void test_res_dir_diffstat()
{
	struct res_dir *r;

	test("RES_DIR: diff / stat");
	r = res_dir_new("dir1");
	res_dir_set(r, "path", DATAROOT "/res_dir/dir1");
	res_dir_set(r, "owner", "someuser");
	r->uid = 1001;
	res_dir_set(r, "group", "staff");
	r->gid = 2002;
	res_dir_set(r, "mode", "0705");

	/* don't need to pass in an env arg (2nd param) */
	assert_int_eq("res_dir_stat returns zero", res_dir_stat(r, NULL), 0);
	assert_int_eq("Directory exists", 1, r->exists);

	assert_true("UID is out of compliance",  DIFFERENT(r, RES_DIR_UID));
	assert_true("GID is out of compliance",  DIFFERENT(r, RES_DIR_GID));
	assert_true("MODE is out of compliance", DIFFERENT(r, RES_DIR_MODE));

	res_dir_free(r);
}

void test_res_dir_fixup_existing()
{
	struct stat st;
	struct res_dir *r;
	struct resource_env env; // needed but not used

	struct report *report;

	const char *path = DATAROOT "/res_dir/fixme";

	test("RES_DIR: Fixup Existing Directory");
	if (stat(path, &st) != 0) {
		assert_fail("RES_FILE: Unable to stat pre-fixup file");
		return;
	}

	assert_int_ne("Pre-fixup: dir owner UID is not 65542",   st.st_uid, 65542);
	assert_int_ne("Pre-fixup: dir group GID is not 65524",   st.st_gid, 65524);
	assert_int_ne("Pre-fixup: dir permissions are not 0754", st.st_mode &07777, 0754);

	r = res_dir_new("fix");
	res_dir_set(r, "path", path);
	res_dir_set(r, "owner", "someuser");
	r->uid = 65542;
	res_dir_set(r, "group", "somegroup");
	r->gid = 65524;
	res_dir_set(r, "mode", "0754");

	assert_int_eq("res_dir_stat succeeds", res_dir_stat(r, &env), 0);
	assert_int_eq("Directory exists", 1, r->exists);
	assert_true("UID is out of compliance", DIFFERENT(r, RES_DIR_UID));
	assert_true("GID is out of compliance", DIFFERENT(r, RES_DIR_GID));

	report = res_dir_fixup(r, 0, &env);
	assert_not_null("res_dir_fixup returns a report", report);
	assert_int_eq("dir was fixed", report->fixed, 1);
	assert_int_eq("dir is now compliant", report->compliant, 1);

	if (stat(path, &st) != 0) {
		assert_fail("Unable to stat post-fixup file");
		return;
	}
	assert_int_eq("Post-fixup: dir owner UID 65542", st.st_uid, 65542);
	assert_int_eq("Post-fixup: dir group GID 65524", st.st_gid, 65524);
	assert_int_eq("Post-fixup: dir permissions are 0754", st.st_mode & 07777, 0754);

	res_dir_free(r);
	report_free(report);
}

void test_res_dir_match()
{
	struct res_dir *rd;

	rd = res_dir_new("tmpdir");
	res_dir_set(rd, "path",  "/tmp");
	res_dir_set(rd, "owner", "root");
	res_dir_set(rd, "group", "sys");
	res_dir_set(rd, "mode",  "1777");

	test("RES_DIR: attribute matching");
	assert_int_eq("tmpdir matches path=/tmp", 0, res_dir_match(rd, "path", "/tmp"));

	assert_int_ne("tmpdir does not match path=/usr", 0, res_dir_match(rd, "path", "/usr"));
	assert_int_ne("owner is not a matchable attr",   0, res_dir_match(rd, "owner", "root"));
	assert_int_ne("group is not a matchable attr",   0, res_dir_match(rd, "group", "sys"));
	assert_int_ne("mode is not a matchable attr",    0, res_dir_match(rd, "mode", "1777"));

	res_dir_free(rd);
}

void test_res_dir_pack()
{
	struct res_dir *rd;
	char *packed;
	const char *expected;

	test("RES_DIR: pack res_dir");
	rd = res_dir_new("home");
	res_dir_set(rd, "path",    "/home");
	res_dir_set(rd, "owner",   "root");
	res_dir_set(rd, "group",   "users");
	res_dir_set(rd, "mode",    "0750");
	res_dir_set(rd, "present", "no");

	packed = res_dir_pack(rd);
	/* mode 0750 (octal) = 000001e8 (hex) */
	expected = "res_dir::\"home\"80000007\"/home\"\"root\"\"users\"000001e8";
	assert_str_eq("packs properly (normal case)", expected, packed);

	res_dir_free(rd);
	free(packed);
}

void test_res_dir_unpack()
{
	struct res_dir *rd;
	const char *packed;

	test("RES_DIR: unpack res_dir");
	packed = "res_dir::\"dirkey\""
		"80000007"
		"\"/tmp\""
		"\"root\""
		"\"sys\""
		"000003ff";
	assert_null("res_dir_unpack returns NULL on failure", res_dir_unpack("<invalid packed data>"));

	rd = res_dir_unpack(packed);
	assert_not_null("res_dir_unpack succeeds", rd);
	assert_str_eq("res_dir->key is \"dirkey\"", "dirkey", rd->key);
	assert_str_eq("res_dir->path is \"/tmp\"", "/tmp", rd->path);

	assert_str_eq("res_dir->owner is \"root\"", "root", rd->owner);
	assert_true("UID is enforced", ENFORCED(rd, RES_DIR_UID));

	assert_str_eq("res_dir->group is \"sys\"", "sys", rd->group);
	assert_true("GID is enforced", ENFORCED(rd, RES_DIR_GID));

	assert_int_eq("res_dir->mode is (oct)1777", 01777, rd->mode);
	assert_true("MODE is enforced", ENFORCED(rd, RES_DIR_MODE));

	assert_true("ABSENT is enforced", ENFORCED(rd, RES_DIR_ABSENT));

	res_dir_free(rd);
}

void test_res_dir_attrs()
{
	struct res_dir *rd;
	struct hash *h;

	h = hash_new();
	test("RES_DIR: attribute retrieval");
	rd = res_dir_new("/tmp");
	assert_int_eq("retrieved res_dir attrs", 0, res_dir_attrs(rd, h));
	assert_attr_hash("rd", h, "path", "/tmp");
	assert_null("h[owner] is NULL (unset)", hash_get(h, "owner"));
	assert_null("h[group] is NULL (unset)", hash_get(h, "group"));
	assert_null("h[mode] is NULL (unset)", hash_get(h, "mode"));
	assert_attr_hash("rd", h, "present", "yes"); // default

	res_dir_set(rd, "owner",   "root");
	res_dir_set(rd, "group",   "sys");
	res_dir_set(rd, "mode",    "01777");
	res_dir_set(rd, "present", "yes");

	assert_int_eq("retrieved res_dir attrs", 0, res_dir_attrs(rd, h));
	assert_attr_hash("rd", h, "path",    "/tmp");
	assert_attr_hash("rd", h, "owner",   "root");
	assert_attr_hash("rd", h, "group",   "sys");
	assert_attr_hash("rd", h, "mode",    "1777");
	assert_attr_hash("rd", h, "present", "yes");

	res_dir_set(rd, "present", "no");
	assert_int_eq("retrieved res_dir attrs", 0, res_dir_attrs(rd, h));
	assert_attr_hash("rd", h, "present", "no");

	assert_int_ne("xyzzy is not a valid attribute", 0, res_dir_set(rd, "xyzzy", "BAD"));
	assert_int_eq("retrieved attrs", 0, res_dir_attrs(rd, h));
	assert_null("h[xyzzy] is NULL (bad attr)", hash_get(h, "xyzzy"));

	hash_free(h);
	res_dir_free(rd);
}

/*****************************************************************************/

void test_suite_resources()
{
	test_resource_keys();
	test_resource_noops();
	test_resource_free_null();
}

void test_suite_res_user()
{
	test_res_user_enforcement();
	test_res_user_diffstat_fixup();
	test_res_user_fixup_new();
	test_res_user_fixup_remove_existing();
	test_res_user_fixup_remove_nonexistent();
	test_res_user_match();
	test_res_user_pack();
	test_res_user_unpack();
	test_res_user_attrs();
}

void test_suite_res_group()
{
	test_res_group_enforcement();
	test_res_group_diffstat_fixup();
	test_res_group_fixup_new();
	test_res_group_fixup_remove_existing();
	test_res_group_fixup_remove_nonexistent();
	test_res_group_match();
	test_res_group_pack();
	test_res_group_unpack();
	test_res_group_add_remove_members_via_set();
	test_res_group_attrs();
	test_res_group_attrs_multivalue();
}

void test_suite_res_file()
{
	test_res_file_enforcement();
	test_res_file_diffstat();
	test_res_file_remedy();
	test_res_file_fixup_new();
	test_res_file_fixup_remove_existing();
	test_res_file_fixup_remove_nonexistent();
	test_res_file_match();
	test_res_file_pack();
	test_res_file_unpack();
	test_res_file_attrs();
}

void test_suite_res_package()
{
	test_res_package_diffstat_fixup();
	test_res_package_match();
	test_res_package_pack();
	test_res_package_unpack();
	test_res_package_attrs();
}

void test_suite_res_service()
{
	test_res_service_diffstat_fixup();
	test_res_service_match();
	test_res_service_pack();
	test_res_service_unpack();
	test_res_service_attrs();
	test_res_service_notify();
}

void test_suite_res_host()
{
	test_res_host_diffstat_fixup();
	test_res_host_match();
	test_res_host_pack();
	test_res_host_unpack();
	test_res_host_attrs();
}

void test_suite_res_sysctl()
{
	test_res_sysctl_diffstat_fixup();
	test_res_sysctl_match();
	test_res_sysctl_pack();
	test_res_sysctl_unpack();
	test_res_sysctl_attrs();
}

void test_suite_res_dir()
{
	test_res_dir_enforcement();
	test_res_dir_diffstat();
	test_res_dir_fixup_existing();
	test_res_dir_match();
	test_res_dir_pack();
	test_res_dir_unpack();
	test_res_dir_attrs();
}
