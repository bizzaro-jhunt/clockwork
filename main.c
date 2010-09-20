#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "res_file.h"
#include "res_group.h"
#include "res_user.h"

/*
 * Print out the details of a res_file structure
 * to standard out, for debugging purposes.
 */
void res_file_dump(struct res_file *rf)
{
	printf("\n\n");
	printf("struct res_file (0x%0x) {\n", (unsigned int)rf);
	printf("  rf_lpath: \"%s\"\n", rf->rf_lpath);
	printf("  rf_rpath: \"%s\"\n", rf->rf_rpath);
	printf("    rf_uid: %u\n", rf->rf_uid);
	printf("    rf_gid: %u\n", rf->rf_gid);
	printf("   rf_mode: %o\n", rf->rf_mode);
	printf("  rf_lsha1: \"%s\"\n", rf->rf_lsha1.hex);
	printf("  rf_rsha1: \"%s\"\n", rf->rf_rsha1.hex);
	printf("-- (rf_stat omitted) --\n");
	printf("    rf_enf: %o\n", rf->rf_enf);
	printf("   rf_diff: %o\n", rf->rf_diff);
	printf("}\n");
	printf("\n");

	printf("UID:  ");
	if (res_file_enforced(rf, UID)) {
		printf("enforced  ");
	} else {
		printf("unenforced");
	}
	printf(" (%02o & %02o == %02o)\n", rf->rf_enf, RES_FILE_UID, rf->rf_enf & RES_FILE_UID);

	printf("GID:  ");
	if (res_file_enforced(rf, GID)) {
		printf("enforced  ");
	} else {
		printf("unenforced");
	}
	printf(" (%02o & %02o == %02o)\n", rf->rf_enf, RES_FILE_GID, rf->rf_enf & RES_FILE_GID);

	printf("MODE: ");
	if (res_file_enforced(rf, MODE)) {
		printf("enforced  ");
	} else {
		printf("unenforced");
	}
	printf(" (%02o & %02o == %02o)\n", rf->rf_enf, RES_FILE_MODE, rf->rf_enf & RES_FILE_MODE);

	printf("SHA1: ");
	if (res_file_enforced(rf, SHA1)) {
		printf("enforced  ");
	} else {
		printf("unenforced");
	}
	printf(" (%02o & %02o == %02o)\n", rf->rf_enf, RES_FILE_SHA1, rf->rf_enf & RES_FILE_SHA1);

	printf("\n");
}

void res_group_dump(struct res_group *rg)
{
	printf("\n\n");
	printf("struct res_group (0x%0x) {\n", (unsigned int)rg);
	printf("    rg_name: \"%s\"\n", rg->rg_name);
	printf("  rg_passwd: \"%s\"\n", rg->rg_passwd);
	printf("     rg_gid: %u\n", rg->rg_gid);
	printf("--- (rg_grp omitted) ---\n");

	printf("      rg_grp: struct passwd {\n");
	printf("                 gr_name: \"%s\"\n", rg->rg_grp.gr_name);
	printf("               gr_passwd: \"%s\"\n", rg->rg_grp.gr_passwd);
	printf("                  gr_gid: %u\n", rg->rg_grp.gr_gid);
	printf("             }\n");

	printf("     rg_enf: %o\n", rg->rg_enf);
	printf("    rg_diff: %o\n", rg->rg_diff);
	printf("}\n");
	printf("\n");

	printf("NAME:   %s (%02o & %02o == %02o)\n",
	       (res_group_enforced(rg, NAME) ? "enforced  " : "unenforced"),
	       rg->rg_enf, RES_GROUP_NAME, rg->rg_enf & RES_GROUP_NAME);
	printf("PASSWD: %s (%02o & %02o == %02o)\n",
	       (res_group_enforced(rg, PASSWD) ? "enforced  " : "unenforced"),
	       rg->rg_enf, RES_GROUP_PASSWD, rg->rg_enf & RES_GROUP_PASSWD);
	printf("GID:    %s (%02o & %02o == %02o)\n",
	       (res_group_enforced(rg, GID) ? "enforced  " : "unenforced"),
	       rg->rg_enf, RES_GROUP_GID, rg->rg_enf & RES_GROUP_GID);
}

void res_user_dump(struct res_user *ru)
{
	printf("\n\n");
	printf("struct res_user (0x%0x) {\n", (unsigned int)ru);
	printf("    ru_name: \"%s\"\n", ru->ru_name);
	printf("  ru_passwd: \"%s\"\n", ru->ru_passwd);
	printf("     ru_uid: %u\n", ru->ru_uid);
	printf("     ru_gid: %u\n", ru->ru_gid);
	printf("   ru_gecos: \"%s\"\n", ru->ru_gecos);
	printf("     ru_dir: \"%s\"\n", ru->ru_dir);
	printf("   ru_shell: \"%s\"\n", ru->ru_shell);
	printf("  ru_mkhome: %u\n", ru->ru_mkhome);
	printf("    ru_lock: %u\n", ru->ru_lock);
	printf("   ru_inact: %li\n", ru->ru_inact);
	printf("  ru_expire: %li\n", ru->ru_expire);

	printf("      ru_pw: struct passwd {\n");
	printf("                 pw_name: \"%s\"\n", ru->ru_pw.pw_name);
	printf("               pw_passwd: \"%s\"\n", ru->ru_pw.pw_passwd);
	printf("                  pw_uid: %u\n", ru->ru_pw.pw_uid);
	printf("                  pw_gid: %u\n", ru->ru_pw.pw_gid);
	printf("                pw_gecos: \"%s\"\n", ru->ru_pw.pw_gecos);
	printf("                  pw_dir: \"%s\"\n", ru->ru_pw.pw_dir);
	printf("                pw_shell: \"%s\"\n", ru->ru_pw.pw_shell);
	printf("             }\n");

	printf("      ru_sp: struct spwd {\n");
	printf("                 sp_namp: \"%s\"\n", ru->ru_sp.sp_namp);
	printf("                 sp_pwdp: \"%s\"\n", ru->ru_sp.sp_pwdp);
	printf("               sp_lstchg: %li\n", ru->ru_sp.sp_lstchg);
	printf("                  sp_min: %li\n", ru->ru_sp.sp_min);
	printf("                  sp_max: %li\n", ru->ru_sp.sp_max);
	printf("                 sp_warn: %li\n", ru->ru_sp.sp_warn);
	printf("                sp_inact: %li\n", ru->ru_sp.sp_inact);
	printf("               sp_expire: %li\n", ru->ru_sp.sp_expire);
	printf("             }\n");

	printf("    ru_skel: \"%s\"\n", ru->ru_skel);
	printf("     ru_enf: %o\n", ru->ru_enf);
	printf("    ru_diff: %o\n", ru->ru_diff);
	printf("}\n");
	printf("\n");

	printf("NAME:   %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, NAME) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_NAME, ru->ru_enf & RES_USER_NAME);
	printf("PASSWD: %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, PASSWD) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_PASSWD, ru->ru_enf & RES_USER_PASSWD);
	printf("UID:    %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, UID) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_UID, ru->ru_enf & RES_USER_UID);
	printf("GID:    %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, GID) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_GID, ru->ru_enf & RES_USER_GID);
	printf("GECOS:  %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, GECOS) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_GECOS, ru->ru_enf & RES_USER_GECOS);
	printf("DIR:    %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, DIR) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_DIR, ru->ru_enf & RES_USER_DIR);
	printf("SHELL:  %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, SHELL) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_SHELL, ru->ru_enf & RES_USER_SHELL);
	printf("MKHOME: %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, MKHOME) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_MKHOME, ru->ru_enf & RES_USER_MKHOME);
	printf("INACT:  %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, INACT) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_INACT, ru->ru_enf & RES_USER_INACT);
	printf("EXPIRE: %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, EXPIRE) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_EXPIRE, ru->ru_enf & RES_USER_EXPIRE);
	printf("LOCK:   %s (%02o & %02o == %02o)\n",
	       (res_user_enforced(ru, LOCK) ? "enforced  " : "unenforced"),
	       ru->ru_enf, RES_USER_LOCK, ru->ru_enf & RES_USER_LOCK);
}

void setup_res_file1(struct res_file *rf1)
{
	res_file_init(rf1);

	rf1->rf_lpath = "test/act/sudoers";
	res_file_set_uid(rf1, 0);
	res_file_set_gid(rf1, 15);
	res_file_set_mode(rf1, 0640);

	res_file_unset_gid(rf1);
}

void setup_res_file2(struct res_file *rf2)
{
	res_file_init(rf2);

	rf2->rf_lpath = "test/act/sudoers";

	if (res_file_set_source(rf2, "test/exp/sudoers") == -1) {
		perror("Unable to generate SHA1 checksum for test/exp/sudoers");
		exit(1);
	}
	res_file_set_uid(rf2, 404);
	res_file_set_gid(rf2, 403);
}

void setup_res_user1(struct res_user *ru1)
{
	res_user_init(ru1);

	res_user_set_name(ru1, "jrh2");
	res_user_set_passwd(ru1, "$6$nahablHe$1qen4PePmYtEIC6aCTYoQFLgMp//snQY7nDGU7.9iVzXrmmCYLDsOKc22J6MPRUuH/X4XJ7w.JaEXjofw9h1d/");
	/*res_user_set_passwd(ru1, "$1$iXn0WLY7$bxZf/GsnpNn0HSsSjba3F1");*/
	res_user_set_uid(ru1, 5000);
	res_user_set_gid(ru1, 1001);
	res_user_set_gecos(ru1, "James Hunt,,,");
	res_user_set_dir(ru1, "/home/super/awesome/jrhunt");
	res_user_set_shell(ru1, "/bin/bash");
	res_user_set_makehome(ru1, 1, "/etc/skel.admin");

}

void setup_res_user2(struct res_user *ru2)
{
	res_user_init(ru2);

	res_user_set_inactivate(ru2, 4);
	res_user_set_expiration(ru2, 815162342);
	res_user_set_lock(ru2, 0);
}

void setup_res_group1(struct res_group *rg1)
{
	res_group_init(rg1);

	res_group_set_name(rg1, "jrhunt");
	res_group_set_gid(rg1, 1005);
	res_group_set_passwd(rg1, "sooper seecrit");
}

void setup_res_group2(struct res_group *rg2)
{
}

int main_test_res_file(int argc, char *argv[])
{
	struct res_file rf1;
	struct res_file rf2;

	setup_res_file1(&rf1);
	rf1.rf_prio = 0;

	setup_res_file2(&rf2);
	rf2.rf_prio = 10; /* lower priority */

	res_file_merge(&rf1, &rf2);

	if (res_file_stat(&rf1) == -1) {
		perror(rf1.rf_lpath);
		return 1;
	}

	res_file_dump(&rf1);

	if (rf1.rf_diff == RES_FILE_NONE) {
		printf("File is in compliance\n");
	} else {
		printf("File is out of compliance:\n");
		printf("         Exp.\tAct.\n");
	}
	if (res_file_different(&rf1, UID)) {
		printf("UID:     %u\t%u\n", (unsigned int)(rf1.rf_uid), (unsigned int)(rf1.rf_stat.st_uid));
	}
	if (res_file_different(&rf1, GID)) {
		printf("GID:     %u\t%u\n", (unsigned int)(rf1.rf_gid), (unsigned int)(rf1.rf_stat.st_gid));
	}
	if (res_file_different(&rf1, MODE)) {
		printf("Mode:    %o\t%o\n", (unsigned int)(rf1.rf_mode), (unsigned int)(rf1.rf_stat.st_mode));
	}
	if (res_file_different(&rf1, SHA1)) {
		printf("SHA1:    %s\t%s\n", rf1.rf_rsha1.hex, rf1.rf_lsha1.hex);
	}

	if (rf1.rf_diff == RES_FILE_NONE) {
		printf("Remediation unnecessary\n");
	} else {
		printf("\n\n");
		printf("Attempting to remediate.\n");

		if (res_file_remediate(&rf1) == -1) {
			perror("rf1 remediation failed");
			exit(1);
		}

		printf("Remediation succeeded!\n");
	}

	res_file_free(&rf1);
	res_file_free(&rf2);

	return 0;
}

int main_test_res_user(int argc, char **argv)
{
	struct res_user ru1;
	struct res_user ru2;

	setup_res_user1(&ru1);
	ru1.ru_prio = 4;

	setup_res_user2(&ru2);
	ru2.ru_prio = 42;

	res_user_merge(&ru1, &ru2);

	if (res_user_stat(&ru1) == -1) {
		perror("unable to stat user");
		exit(1);
	}

	res_user_dump(&ru1);

	if (ru1.ru_diff == RES_USER_NONE) {
		printf("User is in compliance\n");
	} else {
		printf("User is out of compliance:\n");
	}

	printf("         Exp.\tAct.\n");
	if (res_user_different(&ru1, NAME)) {
		printf("NAME:    %s\t%s\n", ru1.ru_name, ru1.ru_pw.pw_name);
	}
	if (res_user_different(&ru1, PASSWD)) {
		printf("PASSWD:  %s\t%s\n", ru1.ru_passwd, ru1.ru_sp.sp_pwdp);
	}
	if (res_user_different(&ru1, UID)) {
		printf("UID:     %u\t%u\n", (unsigned int)(ru1.ru_uid), (unsigned int)(ru1.ru_pw.pw_uid));
	}
	if (res_user_different(&ru1, GID)) {
		printf("GID:     %u\t%u\n", (unsigned int)(ru1.ru_gid), (unsigned int)(ru1.ru_pw.pw_gid));
	}
	if (res_user_different(&ru1, GECOS)) {
		printf("GECOS:   %s\t%s\n", ru1.ru_gecos, ru1.ru_pw.pw_gecos);
	}
	if (res_user_different(&ru1, DIR)) {
		printf("DIR:     %s\t%s\n", ru1.ru_dir, ru1.ru_pw.pw_dir);
	}
	if (res_user_different(&ru1, SHELL)) {
		printf("SHELL:   %s\t%s\n", ru1.ru_shell, ru1.ru_pw.pw_shell);
	}
	if (res_user_different(&ru1, MKHOME)) {
		printf("MKHOME:  X\t-\n");
	}
	if (res_user_different(&ru1, INACT)) {
		printf("INACT:   %li\t%li\n", ru1.ru_inact, ru1.ru_sp.sp_inact);
	}
	if (res_user_different(&ru1, EXPIRE)) {
		printf("EXPIRE:  %li\t%li\n", ru1.ru_expire, ru1.ru_sp.sp_expire);
	}
	if (res_user_different(&ru1, LOCK)) {
		printf("LOCK:    %u\t%u\n", ru1.ru_lock, (ru1.ru_lock ? 0 : 1));
	}

	res_user_free(&ru1);
	res_user_free(&ru2);

	return 0;
}

int main_test_res_group(int argc, char **argv)
{
	struct res_group rg1;
	struct res_group rg2;

	setup_res_group1(&rg1);
	res_group_stat(&rg1);

	res_group_dump(&rg1);

	if (rg1.rg_diff == RES_GROUP_NONE) {
		printf("Group is in compliance\n");
	} else {
		printf("Group is out of compliance:\n");
	}

	printf("         Exp.\tAct.\n");
	if (res_group_different(&rg1, NAME)) {
		printf("NAME:    %s\t%s\n", rg1.rg_name, rg1.rg_grp.gr_name);
	}
	if (res_group_different(&rg1, PASSWD)) {
		printf("PASSWD:  %s\t%s\n", rg1.rg_passwd, rg1.rg_grp.gr_passwd);
	}
	if (res_group_different(&rg1, GID)) {
		printf("GID:     %u\t%u\n", (unsigned int)(rg1.rg_gid), (unsigned int)(rg1.rg_grp.gr_gid));
	}

	res_group_free(&rg1);

	return 0;
}

int main(int argc, char **argv) {
	while (*++argv) {
		     if (strcmp(*argv, "file")  == 0) { main_test_res_file(argc, argv); }
		else if (strcmp(*argv, "user")  == 0) { main_test_res_user(argc, argv); }
		else if (strcmp(*argv, "group") == 0) { main_test_res_group(argc, argv); }
	}
}

// vim:ts=4:noet:sts=4:sw=4
