#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "test.h"
#include "../userdb.h"

/*********************************************************/

#define ABORT_CODE 42

/* for pwdb_init failure-to-open scenarios */
#define BAD_FILE "/non/existent/file"
#define NOT_DIR  "/etc/passwd/is/not/a/directory"

/* test /etc/passwd file */
#define PWFILE     "test/data/passwd"
#define PWFILE_NEW "test/tmp/passwd.new"

/* test /etc/shadow file */
#define SPFILE     "test/data/shadow"
#define SPFILE_NEW "test/tmp/shadow.new"

/* test /etc/group file */
#define GRFILE     "test/data/group"
#define GRFILE_NEW "test/tmp/group.new"

/* test /etc/gshadow file */
#define SGFILE     "test/data/gshadow"
#define SGFILE_NEW "test/tmp/gshadow.new"

/*********************************************************/

static struct pwdb *open_passwd(const char *path)
{
	struct pwdb *db;
	char buf[256];

	db = pwdb_init(path);
	if (!db) {
		if (snprintf(buf, 256, "Unable to open %s", path) < 0) {
			perror("open_passwd / snprintf failed");
		} else {
			perror(buf);
		}
		exit(ABORT_CODE);
	}

	return db;
}

static struct spdb *open_shadow(const char *path)
{
	struct spdb *db;
	char buf[256];

	db = spdb_init(path);
	if (!db) {
		if (snprintf(buf, 256, "Unable to open %s", path) < 0) {
			perror("open_shadow / snprintf failed");
		} else {
			perror(buf);
		}
		exit(ABORT_CODE);
	}

	return db;
}

static struct grdb *open_group(const char *path)
{
	struct grdb *db;
	char buf[256];

	db = grdb_init(path);
	if (!db) {
		if (snprintf(buf, 256, "Unable to open %s", path) < 0) {
			perror("open_group / snprintf failed");
		} else {
			perror(buf);
		}
		exit(ABORT_CODE);
	}

	return db;
}

static struct sgdb *open_gshadow(const char *path)
{
	struct sgdb *db;
	char buf[256];

	db = sgdb_init(path);
	if (!db) {
		if (snprintf(buf, 256, "Unable to open %s", path) < 0) {
			perror("open_gshadow / snprintf failed");
		} else {
			perror(buf);
		}
		exit(ABORT_CODE);
	}

	return db;
}

/*********************************************************/

static void assert_passwd(struct passwd *pw, const char *name, uid_t uid, gid_t gid)
{
	char buf[256];

	snprintf(buf, 256, "%s UID should be %u", name, uid);
	assert_int_equals(buf, pw->pw_uid, uid);

	snprintf(buf, 256, "%s GID should be %u", name, gid);
	assert_int_equals(buf, pw->pw_gid, gid);

	snprintf(buf, 256, "%s username should be %s", name, name);
	assert_str_equals(buf, pw->pw_name, name);
}

static void assert_pwdb_get(struct pwdb *db, const char *name, uid_t uid, gid_t gid)
{
	char buf[256];
	struct passwd *pw;

	pw = pwdb_get_by_name(db, name);

	snprintf(buf, 256, "Look up %s user by name", name);
	assert_not_null(buf, pw);
	assert_passwd(pw, name, uid, gid);

	pw = pwdb_get_by_uid(db, uid);

	snprintf(buf, 256, "Look up %s user by UID (%u)", name, uid);
	assert_not_null(buf, pw);
	assert_passwd(pw, name, uid, gid);
}

static void assert_spdb_get(struct spdb *db, const char *name)
{
	char buf[256];
	struct spwd *sp;

	sp = spdb_get_by_name(db, name);

	snprintf(buf, 256, "Look up %s user by name", name);
	assert_not_null(buf, sp);

	snprintf(buf, 256, "%s username should be %s", name, name);
	assert_str_equals(buf, sp->sp_namp, name);
}

static void assert_group(struct group *gr, const char *name, gid_t gid)
{
	char buf[256];

	snprintf(buf, 256, "%s GID should be %u", name, gid);
	assert_int_equals(buf, gr->gr_gid, gid);

	snprintf(buf, 256, "%s username should be %s", name, name);
	assert_str_equals(buf, gr->gr_name, name);
}

static void assert_grdb_get(struct grdb *db, const char *name, gid_t gid)
{
	char buf[256];
	struct group *gr;

	gr = grdb_get_by_name(db, name);

	snprintf(buf, 256, "Look up %s group by name", name);
	assert_not_null(buf, gr);
	assert_group(gr, name, gid);

	gr = grdb_get_by_gid(db, gid);

	snprintf(buf, 256, "Look up %s group by UID (%u)", name, gid);
	assert_not_null(buf, gr);
	assert_group(gr, name, gid);
}

static void assert_sgdb_get(struct sgdb *db, const char *name)
{
	char buf[256];
	struct sgrp *gr;

	gr = sgdb_get_by_name(db, name);

	snprintf(buf, 256, "Look up %s sgrp by name", name);
	assert_not_null(buf, gr);
	assert_str_equals(buf, gr->sg_namp, name);
}

/*********************************************************/

void test_pwdb_init()
{
	struct pwdb *db;

	test("PWDB: Initialize passwd database structure");

	assert_null("fail to open non-existent file (" BAD_FILE ")", pwdb_init(BAD_FILE));
	assert_null("fail to open bad path (" NOT_DIR ")", pwdb_init(NOT_DIR));
	assert_not_null("open " PWFILE, db = pwdb_init(PWFILE));

	pwdb_free(db);
}

void test_pwdb_get()
{
	struct pwdb *db;
	struct passwd *pw;

	db = open_passwd(PWFILE);

	test("PWDB: Lookup of root (1st passwd entry)");
	assert_pwdb_get(db, "root", 0, 0);

	test("PWDB: Lookup of svc (last passwd entry)");
	assert_pwdb_get(db, "svc", 999, 909);

	test("PWDB: Lookup of user (middle of passwd database)");
	assert_pwdb_get(db, "user", 100, 20);

	pwdb_free(db);
}

void test_pwdb_add()
{
	struct pwdb *db;
	struct passwd pw, *ent;

	db = open_passwd(PWFILE);

	memset(&pw, 0, sizeof(pw));
	test("PWBD: Add an entry to passwd database");
	pw.pw_name = "new_user";
	pw.pw_passwd = "x";
	pw.pw_uid = 500;
	pw.pw_gid = 500;
	pw.pw_gecos = "New User,,,";
	pw.pw_dir = "/home/new_user";
	pw.pw_shell = "/bin/bash";

	assert_null("new_user account doesn't already exist", pwdb_get_by_name(db, pw.pw_name));
	assert_true("creation of new user account", pwdb_add(db, &pw) == 0);
	assert_not_null("new entry exists in memory", pwdb_get_by_name(db, pw.pw_name));
	assert_true("save passwd database to " PWFILE_NEW, pwdb_write(db, PWFILE_NEW) == 0);

	pwdb_free(db);
	db = open_passwd(PWFILE_NEW);
	assert_not_null("re-read new entry from " PWFILE_NEW, ent = pwdb_get_by_name(db, pw.pw_name));

	assert_str_equals("  check equality of pw_name",   "new_user",       ent->pw_name);
	assert_str_equals("  check equality of pw_passwd", "x",              ent->pw_passwd);
	assert_int_equals("  check equality of pw_uid",    500,              ent->pw_uid);
	assert_int_equals("  check equality of pw_gid",    500,              ent->pw_gid);
	assert_str_equals("  check equality of pw_gecos",  "New User,,,",    ent->pw_gecos);
	assert_str_equals("  check equality of pw_dir",    "/home/new_user", ent->pw_dir);
	assert_str_equals("  check equality of pw_shell",  "/bin/bash",      ent->pw_shell);

	if (unlink(PWFILE_NEW) == -1) {
		perror("Unable to remove " PWFILE_NEW);
		exit(ABORT_CODE);
	}

	pwdb_free(db);
}

void test_pwdb_rm()
{
	struct pwdb *db;
	struct passwd *pw;
	const char *rm_user = "sys";

	db = open_passwd(PWFILE);

	test("PWDB: Remove an entry from passwd database");
	assert_not_null("account to be removed exists", pw = pwdb_get_by_name(db, rm_user));
	assert_true("removal of account", pwdb_rm(db, pw) == 0);
	assert_null("removed entry does not exist in memory", pwdb_get_by_name(db, rm_user));
	assert_true("save passwd database to " PWFILE_NEW, pwdb_write(db, PWFILE_NEW) == 0);

	pwdb_free(db);
	db = open_passwd(PWFILE_NEW);

	assert_null("removed entry does not exist in " PWFILE_NEW, pwdb_get_by_name(db, rm_user));

	pwdb_free(db);
}

void test_pwdb_rm_head()
{
	struct pwdb *db;
	struct passwd *pw;
	char *name;

	db = open_passwd(PWFILE);

	test("PWDB: Removal of list head (first entry)");

	pw = db->passwd;
	name = strdup(pw->pw_name);

	assert_true("removal of first account in list", pwdb_rm(db, pw) == 0);
	assert_null("removed entry does not exist in memory", pwdb_get_by_name(db, name));
	assert_true("save passwd database to " PWFILE_NEW, pwdb_write(db, PWFILE_NEW) == 0);

	pwdb_free(db);
	db = open_passwd(PWFILE_NEW);

	assert_null("removed entry does not exist in " PWFILE_NEW, pwdb_get_by_name(db, name));

	free(name);
	pwdb_free(db);
}

void test_pwdb_new_entry()
{
	struct pwdb *db;
	struct passwd *pw;
	char *name = "new_pwuser";

	db = open_passwd(PWFILE);

	test("PWDB: New Entry Creation");

	pw = pwdb_new_entry(db, name);
	assert_not_null("pwdb_new_entry returns a passwd structure", pw);
	assert_str_equals("pw_name is set properly", pw->pw_name, name);
	assert_str_equals("pw_passwd default is sane", pw->pw_passwd, "x");
	assert_int_equals("pw_uid default is sane", pw->pw_uid, -1);
	assert_int_equals("pw_gid default is sane", pw->pw_gid, -1);
	assert_str_equals("pw_gecos default is sane", pw->pw_gecos, "");
	assert_str_equals("pw_dir default is sane", pw->pw_dir, "/");
	assert_str_equals("pw_shell default is sane", pw->pw_shell, "/sbin/nologin");

	pwdb_free(db);
}

void test_spdb_init()
{
	struct spdb *db;

	test("SPDB: Initialize passwd database structure");
	assert_null("fail to open non-existent file (" BAD_FILE ")", spdb_init(BAD_FILE));
	assert_null("fail to open bad path (" NOT_DIR ")", spdb_init(NOT_DIR));
	assert_not_null("open " SPFILE, db = spdb_init(SPFILE));

	spdb_free(db);
}

void test_spdb_get()
{
	struct spdb *db;
	struct spwd *sp;

	db = open_shadow(SPFILE);

	test("SPDB: Lookup of root (1st shadow entry)");
	assert_spdb_get(db, "root");

	test("SPDB: Lookup of svc (last shadow entry)");
	assert_spdb_get(db, "svc");

	test("SPDB: Lookup of user (middle of shadow database)");
	assert_spdb_get(db, "user");

	spdb_free(db);
}

void test_spdb_add()
{
	struct spdb *db;
	struct spwd sp, *ent;

	db = open_shadow(SPFILE);

	memset(&sp, 0, sizeof(sp));
	test("SPDB: Add an entry to shadow database");
	sp.sp_namp = "new_user";
	sp.sp_pwdp = "$6$pwhash";
	sp.sp_lstchg = 14800;
	sp.sp_min = 1;
	sp.sp_max = 6;
	sp.sp_warn = 4;
	sp.sp_inact = 0;
	sp.sp_expire = 0;

	assert_null("new_user entry doesn't already exist", spdb_get_by_name(db, sp.sp_namp));
	assert_true("creation of new shadow entry", spdb_add(db, &sp) == 0);
	assert_not_null("new entry exists in memory", spdb_get_by_name(db, sp.sp_namp));
	assert_true("save shadow database to " SPFILE_NEW, spdb_write(db, SPFILE_NEW) == 0);

	spdb_free(db);
	db = open_shadow(SPFILE_NEW);

	assert_not_null("re-read net entry from " SPFILE_NEW, ent = spdb_get_by_name(db, sp.sp_namp));
	assert_str_equals("  check equality of sp_namp",   "new_user",  ent->sp_namp);
	assert_str_equals("  check equality of sp_pwdp",   "$6$pwhash", ent->sp_pwdp);
	assert_int_equals("  check equality of sp_lstchg", 14800,       ent->sp_lstchg);
	assert_int_equals("  check equality of sp_min",    1,           ent->sp_min);
	assert_int_equals("  check equality of sp_max",    6,           ent->sp_max);
	assert_int_equals("  check equality of sp_warn",   4,           ent->sp_warn);
	assert_int_equals("  check equality of sp_inact",  0,           ent->sp_inact);
	assert_int_equals("  check equality of sp_expire", 0,           ent->sp_expire);

	if (unlink(SPFILE_NEW) == -1) {
		perror("Unable to remove " SPFILE_NEW);
		exit(ABORT_CODE);
	}

	spdb_free(db);
}

void test_spdb_rm()
{
	struct spdb *db;
	struct spwd *sp;
	const char *rm_user = "sys";

	db = open_shadow(SPFILE);

	test("SPDB: Remove an entry from shadow database");
	assert_not_null("account to be removed exists", sp = spdb_get_by_name(db, rm_user));
	assert_true("removal of account", spdb_rm(db, sp) == 0);
	assert_null("removed entry does not exist in memory", spdb_get_by_name(db, rm_user));
	assert_true("save shadow database to " SPFILE_NEW, spdb_write(db, SPFILE_NEW) == 0);

	spdb_free(db);
	db = open_shadow(SPFILE_NEW);

	assert_null("removed entry does not exist in " PWFILE_NEW, spdb_get_by_name(db, rm_user));

	spdb_free(db);
}

void test_spdb_rm_head()
{
	struct spdb *db;
	struct spwd *sp;
	char *name;

	db = open_shadow(SPFILE);

	test("SPDB: Removal of list head (first entry)");

	sp = db->spwd;
	name = strdup(sp->sp_namp);

	assert_true("removal of first account in list", spdb_rm(db, sp) == 0);
	assert_null("removed entry does not exist in memory", spdb_get_by_name(db, name));
	assert_true("save passwd database to " SPFILE_NEW, spdb_write(db, SPFILE_NEW) == 0);

	spdb_free(db);
	db = open_shadow(SPFILE_NEW);

	assert_null("removed entry does not exist in " SPFILE_NEW, spdb_get_by_name(db, name));

	free(name);
	spdb_free(db);
}

void test_spdb_new_entry()
{
	struct spdb *db;
	struct spwd *sp;
	char *name = "new_spuser";

	db = open_shadow(SPFILE);

	test("SPDB: New Entry Creation");

	sp = spdb_new_entry(db, name);
	assert_not_null("spdb_new_entry returns an spwd structure", sp);
	assert_str_equals("sp_namp is set properly", sp->sp_namp, name);
	assert_str_equals("sp_pwdp default is sane", sp->sp_pwdp, "!");
	assert_int_equals("sp_min default is sane", sp->sp_min, 0);
	assert_int_equals("sp_max default is sane", sp->sp_max, 99999);
	assert_int_equals("sp_warn default is sane", sp->sp_warn, 7);
	assert_int_equals("sp_inact default is sane", sp->sp_inact, 0);
	assert_int_equals("sp_expire default is sane", sp->sp_expire, 0);

	spdb_free(db);
}

void test_grdb_init()
{
	struct grdb *db;

	test("GRDB: Initialize group database structure");

	assert_null("fail to open non-existent file (" BAD_FILE ")", grdb_init(BAD_FILE));
	assert_null("fail to open bad path (" NOT_DIR ")", grdb_init(NOT_DIR));
	assert_not_null("open " GRFILE, db = grdb_init(GRFILE));

	grdb_free(db);
}

void test_grdb_get()
{
	struct grdb *db;
	struct group *gr;

	db = open_group(GRFILE);

	test("GRDB: Lookup of root (1st group entry)");
	assert_grdb_get(db, "root", 0);

	test("GRDB: Lookup of svc (last group entry)");
	assert_grdb_get(db, "service", 909);

	test("GRDB: Lookup of users (middle of group database)");
	assert_grdb_get(db, "users", 20);

	grdb_free(db);
}

void test_grdb_add()
{
	struct grdb *db;
	struct group gr, *ent;

	db = open_group(GRFILE);

	memset(&gr, 0, sizeof(gr));
	test("PWBD: Add an entry to group database");
	gr.gr_name = "new_group";
	gr.gr_gid = 500;

	assert_null("new_group account doesn't already exist", grdb_get_by_name(db, gr.gr_name));
	assert_true("creation of new group account", grdb_add(db, &gr) == 0);
	assert_not_null("new entry exists in memory", grdb_get_by_name(db, gr.gr_name));
	assert_true("save group database to " GRFILE_NEW, grdb_write(db, GRFILE_NEW) == 0);

	grdb_free(db);
	db = open_group(GRFILE_NEW);
	assert_not_null("re-read new entry from " GRFILE_NEW, ent = grdb_get_by_name(db, gr.gr_name));

	assert_str_equals("  check equality of gr_name",   "new_group",       ent->gr_name);
	assert_int_equals("  check equality of gr_gid",    500,              ent->gr_gid);

	if (unlink(GRFILE_NEW) == -1) {
		perror("Unable to remove " GRFILE_NEW);
		exit(ABORT_CODE);
	}

	grdb_free(db);
}

void test_grdb_rm()
{
	struct grdb *db;
	struct group *gr;
	const char *rm_group = "sys";

	db = open_group(GRFILE);

	test("GRDB: Remove an entry from group database");
	assert_not_null("account to be removed exists", gr = grdb_get_by_name(db, rm_group));
	assert_true("removal of account", grdb_rm(db, gr) == 0);
	assert_null("removed entry does not exist in memory", grdb_get_by_name(db, rm_group));
	assert_true("save group database to " GRFILE_NEW, grdb_write(db, GRFILE_NEW) == 0);

	grdb_free(db);
	db = open_group(GRFILE_NEW);

	assert_null("removed entry does not exist in " GRFILE_NEW, grdb_get_by_name(db, rm_group));

	if (unlink(GRFILE_NEW) == -1) {
		perror("Unable to remove " GRFILE_NEW);
		exit(ABORT_CODE);
	}

	grdb_free(db);
}

void test_grdb_rm_head()
{
	struct grdb *db;
	struct group *gr;
	char *name;

	db = open_group(GRFILE);

	test("GRDB: Removal of list head (first entry)");

	gr = db->group;
	name = strdup(gr->gr_name);

	assert_true("removal of first account in list", grdb_rm(db, gr) == 0);
	assert_null("removed entry does not exist in memory", grdb_get_by_name(db, name));
	assert_true("save group database to " GRFILE_NEW, grdb_write(db, GRFILE_NEW) == 0);

	grdb_free(db);
	db = open_group(GRFILE_NEW);

	assert_null("removed entry does not exist in " GRFILE_NEW, grdb_get_by_name(db, name));

	if (unlink(GRFILE_NEW) == -1) {
		perror("Unable to remove " GRFILE_NEW);
		exit(ABORT_CODE);
	}

	free(name);
	grdb_free(db);
}

void test_grdb_new_entry()
{
	struct grdb *db;
	struct group *gr;
	char *name = "new_group";

	db = open_group(GRFILE);

	test("GRDB: New Entry Creation");

	gr = grdb_new_entry(db, name);
	assert_not_null("grdb_new_entry returns a group structure", gr);
	assert_str_equals("gr_name is set properly", gr->gr_name, name);
	assert_str_equals("gr_passwd default is sane", gr->gr_passwd, "x");
	assert_int_equals("gr_gid default is sane", gr->gr_gid, -1);

	grdb_free(db);
}

void test_sgdb_init()
{
	struct sgdb *db;

	test("SGDB: Initialize passwd database structure");
	assert_null("fail to open non-existent file (" BAD_FILE ")", sgdb_init(BAD_FILE));
	assert_null("fail to open bad path (" NOT_DIR ")", sgdb_init(NOT_DIR));
	assert_not_null("open " SGFILE, db = sgdb_init(SGFILE));

	sgdb_free(db);
}

void test_sgdb_get()
{
	struct sgdb *db;
	struct sgrp *sp;

	db = open_gshadow(SGFILE);

	test("SGDB: Lookup of root (1st gshadow entry)");
	assert_sgdb_get(db, "root");

	test("SGDB: Lookup of service (last gshadow entry)");
	assert_sgdb_get(db, "service");

	test("SGDB: Lookup of user (middle of gshadow database)");
	assert_sgdb_get(db, "sys");

	sgdb_free(db);
}

void test_sgdb_add()
{
	struct sgdb *db;
	struct sgrp sp, *ent;

	db = open_gshadow(SGFILE);

	memset(&sp, 0, sizeof(sp));
	test("SGDB: Add an entry to gshadow database");
	sp.sg_namp = "new_group";
	sp.sg_passwd = "$6$pwhash";

	assert_null("new_group entry doesn't already exist", sgdb_get_by_name(db, sp.sg_namp));
	assert_true("creation of newg shadow entry", sgdb_add(db, &sp) == 0);
	assert_not_null("new entry exists in memory", sgdb_get_by_name(db, sp.sg_namp));
	assert_true("save gshadow database to " SGFILE_NEW, sgdb_write(db, SGFILE_NEW) == 0);

	sgdb_free(db);
	db = open_gshadow(SGFILE_NEW);

	assert_not_null("re-read net entry from " SGFILE_NEW, ent = sgdb_get_by_name(db, sp.sg_namp));
	assert_str_equals("  check equality of sg_namp",   "new_group",  ent->sg_namp);
	assert_str_equals("  check equality of sg_passwd", "$6$pwhash", ent->sg_passwd);

	if (unlink(SGFILE_NEW) == -1) {
		perror("Unable to remove " SGFILE_NEW);
		exit(ABORT_CODE);
	}

	sgdb_free(db);
}

void test_sgdb_rm()
{
	struct sgdb *db;
	struct sgrp *sp;
	const char *rm_user = "sys";

	db = open_gshadow(SGFILE);

	test("SGDB: Remove an entry from gshadow database");
	assert_not_null("account to be removed exists", sp = sgdb_get_by_name(db, rm_user));
	assert_true("removal of account", sgdb_rm(db, sp) == 0);
	assert_null("removed entry does not exist in memory", sgdb_get_by_name(db, rm_user));
	assert_true("save gshadow database to " SGFILE_NEW, sgdb_write(db, SGFILE_NEW) == 0);

	sgdb_free(db);
	db = open_gshadow(SGFILE_NEW);

	assert_null("removed entry does not exist in " PWFILE_NEW, sgdb_get_by_name(db, rm_user));

	sgdb_free(db);
}

void test_sgdb_rm_head()
{
	struct sgdb *db;
	struct sgrp *sp;
	char *name;

	db = open_gshadow(SGFILE);

	test("SGDB: Removal of list head (first entry)");

	sp = db->sgrp;
	name = strdup(sp->sg_namp);

	assert_true("removal of first account in list", sgdb_rm(db, sp) == 0);
	assert_null("removed entry does not exist in memory", sgdb_get_by_name(db, name));
	assert_true("save passwd database to " SGFILE_NEW, sgdb_write(db, SGFILE_NEW) == 0);

	sgdb_free(db);
	db = open_gshadow(SGFILE_NEW);

	assert_null("removed entry does not exist in " SGFILE_NEW, sgdb_get_by_name(db, name));

	free(name);
	sgdb_free(db);
}

void test_sgdb_new_entry()
{
	struct sgdb *db;
	struct sgrp *sg;
	char *name = "new_group";

	db = open_gshadow(SGFILE);

	test("SGDB: New Entry Creation");

	sg = sgdb_new_entry(db, name);
	assert_not_null("sgdb_new_entry returns an sgrp structure", sg);
	assert_str_equals("sg_namp is set properly", sg->sg_namp, name);

	sgdb_free(db);
}

void test_suite_userdb() {

	/* passwd db tests */
	test_pwdb_init();
	test_pwdb_get();
	test_pwdb_add();
	test_pwdb_rm();
	test_pwdb_rm_head();
	test_pwdb_new_entry();

	/* shadow db tests */
	test_spdb_init();
	test_spdb_get();
	test_spdb_add();
	test_spdb_rm();
	test_spdb_rm_head();
	test_spdb_new_entry();

	/* group db tests */
	test_grdb_init();
	test_grdb_get();
	test_grdb_add();
	test_grdb_rm();
	test_grdb_rm_head();
	test_grdb_new_entry();

	/* gshadow db tests */
	test_sgdb_init();
	test_sgdb_get();
	test_sgdb_add();
	test_sgdb_rm();
	test_sgdb_rm_head();
	test_sgdb_new_entry();
}
