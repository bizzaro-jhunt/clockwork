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

#include <unistd.h>

#include "test.h"
#include "../src/userdb.h"

/*********************************************************/

#define ABORT_CODE 42

/* for pwdb_init failure-to-open scenarios */
#define BAD_FILE    "/non/existent/file"
#define NOT_DIR     "/etc/passwd/is/not/a/directory"

/* test /etc/passwd file */
#define PWFILE      TEST_DATA "/userdb/passwd"
#define PWFILE_UID  TEST_DATA "/userdb/passwd-uid"
#define PWFILE_NEW  TEST_TMP  "/passwd"

/* test /etc/shadow file */
#define SPFILE      TEST_DATA "/userdb/shadow"
#define SPFILE_NEW  TEST_TMP  "/shadow"

/* test /etc/group file */
#define GRFILE      TEST_DATA "/userdb/group"
#define GRFILE_NEW  TEST_TMP  "/group"

/* test /etc/gshadow file */
#define SGFILE      TEST_DATA "/userdb/gshadow"
#define SGFILE_NEW  TEST_TMP  "/gshadow"

/*********************************************************/

TESTS {
	subtest {
		struct pwdb *db;
		struct passwd *pw;

		is_null(pwdb_init(BAD_FILE), "passwd init filed for non-existent file " BAD_FILE);
		is_null(pwdb_init(NOT_DIR),  "passwd init filed for bad path " NOT_DIR);
		isnt_null(db = pwdb_init(PWFILE), "open " PWFILE);

		// root is the first entry in passwd
		isnt_null(pw = pwdb_get_by_name(db, "root"), "lookup root by name");
		if (!pw) break;
		is_int(pw->pw_uid, 0, "root UID");
		is_int(pw->pw_gid, 0, "root GID");
		is_string(pw->pw_name, "root", "root username");
		isnt_null(pw = pwdb_get_by_uid(db, 0), "lookup root by UID");
		if (!pw) break;
		is_int(pw->pw_uid, 0, "root UID");
		is_int(pw->pw_gid, 0, "root GID");
		is_string(pw->pw_name, "root", "root username");

		// svc is the last entry in passwd
		isnt_null(pw = pwdb_get_by_name(db, "svc"), "lookup svc by name");
		if (!pw) break;
		is_int(pw->pw_uid, 999, "svc UID");
		is_int(pw->pw_gid, 909, "svc GID");
		is_string(pw->pw_name, "svc", "svc username");
		isnt_null(pw = pwdb_get_by_uid(db, 999), "lookup svc by UID");
		if (!pw) break;
		is_int(pw->pw_uid, 999, "svc UID");
		is_int(pw->pw_gid, 909, "svc GID");
		is_string(pw->pw_name, "svc", "svc username");

		// 'user' is in the middle of passwd
		isnt_null(pw = pwdb_get_by_name(db, "user"), "lookup user by name");
		if (!pw) break;
		is_int(pw->pw_uid, 100, "user UID");
		is_int(pw->pw_gid,  20, "user GID");
		is_string(pw->pw_name, "user", "user username");
		isnt_null(pw = pwdb_get_by_uid(db, 100), "lookup user by UID");
		if (!pw) break;
		is_int(pw->pw_uid, 100, "user UID");
		is_int(pw->pw_gid,  20, "user GID");
		is_string(pw->pw_name, "user", "user username");

		pwdb_free(db);
	}

	subtest {
		struct pwdb *db;
		struct passwd pw, *ent;

		db = pwdb_init(PWFILE);

		memset(&pw, 0, sizeof(pw));
		pw.pw_name   = "new_user";
		pw.pw_passwd = "x";
		pw.pw_uid    = 500;
		pw.pw_gid    = 500;
		pw.pw_gecos  = "New User,,,";
		pw.pw_dir    = "/home/new_user";
		pw.pw_shell  = "/bin/bash";

		is_null(pwdb_get_by_name(db, pw.pw_name), "doesnt exist yet");
		ok(pwdb_add(db, &pw) == 0, "created user account");
		isnt_null(pwdb_get_by_name(db, pw.pw_name), "new entry exists in memory");
		ok(pwdb_write(db, PWFILE_NEW) == 0, "wrote new passwd db to " PWFILE_NEW);

		pwdb_free(db);
		db = pwdb_init(PWFILE_NEW);
		isnt_null(ent = pwdb_get_by_name(db, pw.pw_name), "reread entry");

		is_string( ent->pw_name,   "new_user",       "new user username");
		is_string( ent->pw_passwd, "x",              "new user password field");
		is_int(    ent->pw_uid,    500,              "new user UID");
		is_int(    ent->pw_gid,    500,              "new user GID");
		is_string( ent->pw_gecos,  "New User,,,",    "new user GECOS/comment");
		is_string( ent->pw_dir,    "/home/new_user", "new user home directory");
		is_string( ent->pw_shell,  "/bin/bash",      "new user shell");

		sys("rm -f " PWFILE_NEW);

		pwdb_free(db);
	}

	subtest {
		struct pwdb *db;
		struct passwd *pw;
		const char *rm_user = "sys";

		isnt_null(db = pwdb_init(PWFILE), "read passwd");

		// Remove an entry from passwd database
		isnt_null(pw = pwdb_get_by_name(db, rm_user), "found user to remove");
		ok(pwdb_rm(db, pw) == 0, "removed account");
		is_null(pwdb_get_by_name(db, rm_user), "removed user no longer exists");
		ok(pwdb_write(db, PWFILE_NEW) == 0, "saved passwd");

		pwdb_free(db);

		isnt_null(db = pwdb_init(PWFILE_NEW), "read passwd");
		is_null(pwdb_get_by_name(db, rm_user), "removal reflected on-disk");

		pwdb_free(db);
	}

	subtest {
		struct pwdb *db;
		struct passwd *pw;
		char *name;

		isnt_null(db = pwdb_init(PWFILE), "read passwd");

		// Removal of list head (first entry)

		pw = db->passwd; // list head
		name = strdup(pw->pw_name);

		ok(pwdb_rm(db, pw) == 0, "removed first account in list");
		is_null(pwdb_get_by_name(db, name), "removed account does not exist in memory");
		ok(pwdb_write(db, PWFILE_NEW) == 0, "saved passwd");
		pwdb_free(db);

		isnt_null(db = pwdb_init(PWFILE_NEW), "read passwd");
		is_null(pwdb_get_by_name(db, name), "removal reflected on-disk");

		free(name);
		pwdb_free(db);
	}

	subtest {
		struct pwdb *db;
		struct passwd *pw;

		// Test entry default values
		isnt_null(db = pwdb_init(PWFILE), "read passwd");
		isnt_null(pw = pwdb_new_entry(db, "new_pwuser", 1001, 1002), "created new passwd entry");

		is_string(pw->pw_name,   "new_pwuser",    "explicitly set username");
		is_int(   pw->pw_uid,    1001,            "explicitly set UID");
		is_int(   pw->pw_gid,    1002,            "explicitly set GID");
		is_string(pw->pw_passwd, "x",             "default password");
		is_string(pw->pw_gecos,  "",              "default GECOS/comment");
		is_string(pw->pw_dir,    "/",             "default home directory");
		is_string(pw->pw_shell,  "/sbin/nologin", "default shell");

		pwdb_free(db);
	}

	subtest {
		struct pwdb *db;
		// Next UID out of order (not just last(UID)+1
		isnt_null(db = pwdb_init(PWFILE_UID), "read passwd");
		is_int(pwdb_next_uid(db), 8192, "Next UUID == max(UID)+1");
		pwdb_free(db);
	}

	subtest {
		struct pwdb *db;
		uid_t uid = 42;

		isnt_null(db = pwdb_init(PWFILE_UID), "read passwd");

		is_int(pwdb_lookup_uid(db, NULL, &uid), -1, "lookup UID with NULL username");
		is_int(uid, 42, "uid value-result arg is untouched");

		is_int(pwdb_lookup_uid(db, "notauser", &uid), -1, "lookup UID with username");
		is_int(uid, 42, "uid value-result arg is untouched");

		// see test/data/passwd-uid for valid users
		is_int(pwdb_lookup_uid(db, "jrh1", &uid), 0, "lookup UID for jrh1 account");
		is_int(uid, 2048, "UID for jrh1 account");

		pwdb_free(db);
	}


	subtest {
		struct spdb *db;
		struct spwd *sp;

		is_null(spdb_init(BAD_FILE), "open bad shadow file " BAD_FILE);
		is_null(spdb_init(NOT_DIR),  "open bad shadow file " NOT_DIR);
		isnt_null(db = spdb_init(SPFILE), "open " SPFILE);

		// first shadow entry
		isnt_null(sp = spdb_get_by_name(db, "root"), "lookup root user");
		is_string(sp->sp_namp, "root", "shadow entry username");

		// last shadow entry
		isnt_null(sp = spdb_get_by_name(db, "svc"), "lookup svc user");
		is_string(sp->sp_namp, "svc", "shadow entry username");

		// middle of the /etc/shadow db
		isnt_null(sp = spdb_get_by_name(db, "user"), "lookup user user");
		is_string(sp->sp_namp, "user", "shadow entry username");

		spdb_free(db);
	}

	subtest {
		struct spdb *db;
		struct spwd sp, *ent;

		isnt_null(db = spdb_init(SPFILE), "read shadow db");

		memset(&sp, 0, sizeof(sp));
		sp.sp_namp   = "new_user";
		sp.sp_pwdp   = "$6$pwhash";
		sp.sp_lstchg = 14800;
		sp.sp_min    = 1;
		sp.sp_max    = 6;
		sp.sp_warn   = 4;
		sp.sp_inact  = 0;
		sp.sp_expire = 0;

		is_null(spdb_get_by_name(db, "new_user"), "new_user doesnt exist yet");
		ok(spdb_add(db, &sp) == 0, "created new shadow entry");
		isnt_null(spdb_get_by_name(db, "new_user"), "new_user exists now");
		ok(spdb_write(db, SPFILE_NEW) == 0, "saved shadow db to " SPFILE_NEW);

		spdb_free(db);
		isnt_null(db = spdb_init(SPFILE_NEW), "read shadow db");

		isnt_null(ent = spdb_get_by_name(db, "new_user"), "re-read new shadow entry");
		is_string(ent->sp_namp,   "new_user",  "new user username");
		is_string(ent->sp_pwdp,   "$6$pwhash", "new user password hash");
		is_int(   ent->sp_lstchg, 14800,       "last-changed value");
		is_int(   ent->sp_min,    1,           "min password age");
		is_int(   ent->sp_max,    6,           "max password age");
		is_int(   ent->sp_warn,   4,           "pw change warning");
		is_int(   ent->sp_inact,  0,           "account inactivity deadline");
		is_int(   ent->sp_expire, 0,           "account expiry");

		sys("rm -f " SPFILE_NEW);
		spdb_free(db);
	}

	subtest {
		struct spdb *db;
		struct spwd *sp;
		const char *rm_user = "sys";

		isnt_null(db = spdb_init(SPFILE), "read shadow db");
		isnt_null(sp = spdb_get_by_name(db, rm_user), "user account exists");
		ok(spdb_rm(db, sp) == 0, "removed account");
		is_null(spdb_get_by_name(db, rm_user), "user account not found in-memory");
		ok(spdb_write(db, SPFILE_NEW) == 0, "saved shadow db");
		spdb_free(db);

		isnt_null(db = spdb_init(SPFILE_NEW), "read shadow db");
		is_null(spdb_get_by_name(db, rm_user), "removal reflected on-disk");
		spdb_free(db);
	}

	subtest {
		struct spdb *db;
		struct spwd *sp;
		char *name;

		isnt_null(db = spdb_init(SPFILE), "read shadow db");
		sp = db->spwd;
		name = strdup(sp->sp_namp);

		ok(spdb_rm(db, sp) == 0, "removed first account");
		is_null(spdb_get_by_name(db, name), "user account not found in-memory");
		ok(spdb_write(db, SPFILE_NEW) == 0, "saved shadow db");
		spdb_free(db);

		isnt_null(db = spdb_init(SPFILE_NEW), "read shadow db");
		is_null(spdb_get_by_name(db, name), "removal reflected on-disk");
		free(name);
		spdb_free(db);
	}

	subtest {
		struct spdb *db;
		struct spwd *sp;

		isnt_null(db = spdb_init(SPFILE), "read shadow db");
		isnt_null(sp = spdb_new_entry(db, "new_spuser"), "created new shadow entry");

		is_string(sp->sp_namp, "new_spuser", "explicitly set username");
		is_string(sp->sp_pwdp,          "!", "default hashed password");
		is_int(   sp->sp_min,            -1, "default min password age");
		is_int(   sp->sp_max,         99999, "default min password age");
		is_int(   sp->sp_warn,            7, "default pw change warning");
		is_int(   sp->sp_inact,          -1, "default inactivity deadline");
		is_int(   sp->sp_expire,         -1, "default account expiry");
		is_unsigned(sp->sp_flag,          0, "default flags");

		spdb_free(db);
	}


	subtest {
		struct grdb *db;
		struct group *gr;

		is_null(grdb_init(BAD_FILE), "open bad group file " BAD_FILE);
		is_null(grdb_init(NOT_DIR),  "open bad group file " NOT_DIR);
		isnt_null(db = grdb_init(GRFILE), "open " GRFILE);

		// first group entry
		isnt_null(gr = grdb_get_by_name(db, "root"), "found root group by name");
		is_int(gr->gr_gid, 0, "root group GID");
		is_string(gr->gr_name, "root", "root group name");
		isnt_null(gr = grdb_get_by_gid(db, 0), "found root group by GID");
		is_int(gr->gr_gid, 0, "root group GID");
		is_string(gr->gr_name, "root", "root group name");

		// last group entry
		isnt_null(gr = grdb_get_by_name(db, "service"), "found service group by name");
		is_int(gr->gr_gid, 909, "service group GID");
		is_string(gr->gr_name, "service", "service group name");
		isnt_null(gr = grdb_get_by_gid(db, 909), "found service group by GID");
		is_int(gr->gr_gid, 909, "service group GID");
		is_string(gr->gr_name, "service", "service group name");

		// middle group entry
		isnt_null(gr = grdb_get_by_name(db, "users"), "found users group by name");
		is_int(gr->gr_gid, 20, "users group GID");
		is_string(gr->gr_name, "users", "users group name");
		isnt_null(gr = grdb_get_by_gid(db, 20), "found users group by GID");
		is_int(gr->gr_gid, 20, "users group GID");
		is_string(gr->gr_name, "users", "users group name");

		grdb_free(db);
	}

	subtest {
		struct grdb *db;
		struct group gr, *ent;

		isnt_null(db = grdb_init(GRFILE), "read group db");

		memset(&gr, 0, sizeof(gr));
		gr.gr_name = "new_group";
		gr.gr_gid  = 500;

		is_null(grdb_get_by_name(db, "new_group"), "group doesnt exist yet");
		ok(grdb_add(db, &gr) == 0, "created group");
		isnt_null(grdb_get_by_name(db, "new_group"), "group exists now");
		ok(grdb_write(db, GRFILE_NEW) == 0, "saved group db");
		grdb_free(db);

		isnt_null(db = grdb_init(GRFILE_NEW), "read group db");
		isnt_null(ent = grdb_get_by_name(db, "new_group"), "found group account");

		is_string(ent->gr_name, "new_group", "group name");
		is_int(   ent->gr_gid,  500,         "group GID");

		sys("rm -f " GRFILE_NEW);
		grdb_free(db);
	}

	subtest {
		struct grdb *db;
		struct group *gr;

		isnt_null(db = grdb_init(GRFILE), "read group db");

		isnt_null(gr = grdb_get_by_name(db, "sys"), "group exists");
		ok(grdb_rm(db, gr) == 0, "removed group");
		is_null(gr = grdb_get_by_name(db, "sys"), "group no longer exists");
		ok(grdb_write(db, GRFILE_NEW) == 0, "wrote group db");
		grdb_free(db);

		isnt_null(db = grdb_init(GRFILE_NEW), "read group db");
		is_null(grdb_get_by_name(db, "sys"), "removal reflected on-disk");

		sys("rm -f " GRFILE_NEW);
		grdb_free(db);
	}

	subtest {
		struct grdb *db;
		struct group *gr;
		char *name;

		isnt_null(db = grdb_init(GRFILE), "read group db");
		isnt_null(gr = db->group, "found first group");
		name = strdup(gr->gr_name);

		ok(grdb_rm(db, gr) == 0, "removed first group");
		is_null(grdb_get_by_name(db, name), "removed group does not exist in-memory");
		ok(grdb_write(db, GRFILE_NEW) == 0, "wrote group db");
		grdb_free(db);

		isnt_null(db = grdb_init(GRFILE_NEW), "read group db");
		is_null(grdb_get_by_name(db, name), "removal reflected on-disk");

		sys("rm -f " GRFILE_NEW);
		free(name);
		grdb_free(db);
	}

	subtest {
		struct grdb *db;
		struct group *gr;

		isnt_null(db = grdb_init(GRFILE), "read group db");
		isnt_null(gr = grdb_new_entry(db, "new_group", 2002), "created new group entry");

		is_string(gr->gr_name,   "new_group", "explicitly set group name");
		is_int(   gr->gr_gid,           2002, "explicitly set group GID");
		is_string(gr->gr_passwd,         "x", "default group password");

		grdb_free(db);
	}

	subtest {
		struct grdb *db;
		struct group *gr;

		isnt_null(db = grdb_init(GRFILE), "read group db");
		isnt_null(gr = grdb_get_by_name(db, "members"), "found 'members' group");
		isnt_null(gr->gr_mem, "'members' group has a gr_mem array");

		is_string(gr->gr_mem[0], "account1", "gr_mem[0]");
		is_string(gr->gr_mem[1], "account2", "gr_mem[1]");
		is_string(gr->gr_mem[2], "account3", "gr_mem[2]");
		is_null(gr->gr_mem[3], "no more members");

		grdb_free(db);
	}

	subtest {
		struct grdb *db;
		gid_t gid = 42;

		isnt_null(db = grdb_init(GRFILE), "read group db");

		is_int(grdb_lookup_gid(db, NULL, &gid), -1, "lookup group with NULL group name");
		is_int(gid, 42, "gid value-result arg is untouched");

		is_int(grdb_lookup_gid(db, "notagroup", &gid), -1, "lookup GID with bad group name");
		is_int(gid, 42, "gid value-result arg is untouched");

		// see test/data/group for group definitions
		is_int(grdb_lookup_gid(db, "service", &gid), 0, "lookup group 'service'");
		is_int(gid, 909, "GID for group 'service'");

		grdb_free(db);
	}


	subtest {
		struct sgdb *db;
		struct sgrp *sg;

		is_null(sgdb_init(BAD_FILE), "passwd init filed for non-existent file " BAD_FILE);
		is_null(sgdb_init(NOT_DIR),  "passwd init filed for bad path " NOT_DIR);
		isnt_null(db = sgdb_init(SGFILE), "open " SGFILE);

		// first gshadow entry
		isnt_null(sg = sgdb_get_by_name(db, "root"), "found root gshadow entry");
		is_string(sg->sg_namp, "root", "ghadow group name");

		// last gshadow entry
		isnt_null(sg = sgdb_get_by_name(db, "service"), "found service gshadow entry");
		is_string(sg->sg_namp, "service", "ghadow group name");

		// middle of gshadow db
		isnt_null(sg = sgdb_get_by_name(db, "sys"), "found sys gshadow entry");
		is_string(sg->sg_namp, "sys", "ghadow group name");

		sgdb_free(db);
	}

	subtest {
		struct sgdb *db;
		struct sgrp sp, *ent;

		isnt_null(db = sgdb_init(SGFILE), "read gshadow db");

		memset(&sp, 0, sizeof(sp));
		sp.sg_namp   = "new_group";
		sp.sg_passwd = "$6$pwhash";

		is_null(sgdb_get_by_name(db, "new_group"), "gshadow entry does exist yet");
		ok(sgdb_add(db, &sp) == 0, "added gshadow entry");
		isnt_null(sgdb_get_by_name(db, "new_group"), "gshadow entry exists now");
		ok(sgdb_write(db, SGFILE_NEW) == 0, "wrote gshadow to " SGFILE_NEW);
		sgdb_free(db);

		isnt_null(db = sgdb_init(SGFILE_NEW), "read gshadow db");
		isnt_null(ent = sgdb_get_by_name(db, "new_group"), "found new gshadow entry");
		is_string(ent->sg_namp,   "new_group", "gshadow entry group name");
		is_string(ent->sg_passwd, "$6$pwhash", "gshadow entry password hash");

		sys("rm -f " SGFILE_NEW);
		sgdb_free(db);
	}

	subtest {
		struct sgdb *db;
		struct sgrp *sp;

		isnt_null(db = sgdb_init(SGFILE), "read gshadow db");
		isnt_null(sp = sgdb_get_by_name(db, "sys"), "gshadow entry 'sys' does not exist yet");
		ok(sgdb_rm(db, sp) == 0, "removed account");
		is_null(sp = sgdb_get_by_name(db, "sys"), "gshadow entry 'sys' no longer exists");
		ok(sgdb_write(db, SGFILE_NEW) == 0, "wrote gshadow db");
		sgdb_free(db);

		isnt_null(db = sgdb_init(SGFILE_NEW), "read gshadow db");
		is_null(sp = sgdb_get_by_name(db, "sys"), "removal of entry reflected on-disk");

		sgdb_free(db);
	}

	subtest {
		struct sgdb *db;
		struct sgrp *sp;
		char *name;

		isnt_null(db = sgdb_init(SGFILE), "read gshadow db");
		isnt_null(sp = db->sgrp, "found first gshadow entry");
		name = strdup(sp->sg_namp);

		ok(sgdb_rm(db, sp) == 0, "removed first account");
		ok(sgdb_write(db, SGFILE_NEW) == 0, "wrote gshadow db");
		sgdb_free(db);

		isnt_null(db = sgdb_init(SGFILE_NEW), "read gshadow db");
		is_null(sp = sgdb_get_by_name(db, name), "removal of entry reflected on-disk");

		free(name);
		sgdb_free(db);
	}

	subtest {
		struct sgdb *db;
		struct sgrp *sg;

		isnt_null(db = sgdb_init(SGFILE), "read gshadow db");
		isnt_null(sg = sgdb_new_entry(db, "new_group"), "created new entry");
		is_string(sg->sg_namp, "new_group", "explicitly set gshadow group name");

		sgdb_free(db);
	}

	subtest {
		struct sgdb *db;
		struct sgrp *sg;

		isnt_null(db = sgdb_init(SGFILE), "read gshadow db");
		isnt_null(sg = sgdb_get_by_name(db, "members"), "found the 'members' gshadow entry");

		isnt_null(sg->sg_mem, "'members' gshadow entry has list of members");
		is_string(sg->sg_mem[0], "account1", "sg_mem[0]");
		is_string(sg->sg_mem[1], "account2", "sg_mem[1]");
		is_string(sg->sg_mem[2], "account3", "sg_mem[2]");
		is_null(sg->sg_mem[3], "no more members");

		isnt_null(sg->sg_adm, "'members' gshadow entry has list of admins");
		is_string(sg->sg_adm[0], "admin1", "sg_adm[0]");
		is_string(sg->sg_adm[1], "admin2", "sg_adm[1]");
		is_null(sg->sg_adm[2], "no more admins");

		sgdb_free(db);
	}

	done_testing();
}
