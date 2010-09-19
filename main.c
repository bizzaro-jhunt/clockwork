#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "res_file.h"
#include "res_group.h"
#include "res_user.h"

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

	res_user_set_name(ru1, "jrhunt");
	res_user_set_passwd(ru1, "*");
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
	res_user_stat(&ru1);

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
		printf("PASSWD:  %s\t%s\n", ru1.ru_passwd, ru1.ru_pw.pw_passwd);
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

	res_user_free(&ru1);

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
