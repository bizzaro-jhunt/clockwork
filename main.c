#include <stdio.h>
#include <errno.h>
#include <assert.h>

#include "res_file.h"

void setup_res_file1(struct res_file *rf1)
{
	res_file_init(rf1);

	rf1->rf_lpath = "/etc/sudoers";
	res_file_set_uid(rf1, 0);
	res_file_set_gid(rf1, 15);
	res_file_set_mode(rf1, 0640);

	res_file_unset_gid(rf1);
}

void setup_res_file2(struct res_file *rf2)
{
	res_file_init(rf2);

	rf2->rf_lpath = "/etc/sudoers";

	if (res_file_set_source(rf2, "test/sudoers") == -1) {
		perror("Unable to generate SHA1 checksum for test/sudoers");
		exit(1);
	}
	res_file_set_uid(rf2, 404);
	res_file_set_gid(rf2, 403);
}

int main(int argc, char *argv[])
{
	struct res_file rf1;
	struct res_file rf2;

	setup_res_file1(&rf1);
	rf1.rf_prio = 0;

	setup_res_file2(&rf2);
	rf2.rf_prio = 10; /* lower priority */

	/*
	res_file_dump(&rf1);
	res_file_dump(&rf2);
	*/

	res_file_merge(&rf1, &rf2);

	/*
	res_file_dump(&rf1);
	*/

	if (res_file_stat(&rf1) == -1) {
		perror(rf1.rf_lpath);
		return 1;
	}

	if (rf1.rf_diff == RES_FILE_NONE) {
		printf("File is in compliance\n");
	} else {
		printf("File is out of compliance:\n");
	}
	printf("         Exp.\tAct.\n");
	if (res_file_different(&rf1, UID)) {
		printf("UID:     %u\t%u\n", (unsigned int)(rf1.rf_uid), (unsigned int)(rf1.rf_stat->st_uid));
	}
	if (res_file_different(&rf1, GID)) {
		printf("GID:     %u\t%u\n", (unsigned int)(rf1.rf_gid), (unsigned int)(rf1.rf_stat->st_gid));
	}
	if (res_file_different(&rf1, MODE)) {
		printf("Mode:    %o\t%o\n", (unsigned int)(rf1.rf_mode), (unsigned int)(rf1.rf_stat->st_mode));
	}

	if (res_file_different(&rf1, SHA1)) {
		printf("SHA1:    %s\t%s\n", rf1.rf_rsha1.hex, rf1.rf_lsha1.hex);
	}

	res_file_free(&rf1);
	res_file_free(&rf2);

	return 0;
}

// vim:ts=4:noet:
