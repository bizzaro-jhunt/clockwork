#ifndef RES_FILE_H
#define RES_FILE_H

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "sha1.h"

#define RES_FILE_NONE 0000
#define RES_FILE_UID  0001
#define RES_FILE_GID  0002
#define RES_FILE_MODE 0004
#define RES_FILE_SHA1 0010

#define res_file_enforced(rf, flag)  (((rf)->rf_enf  & RES_FILE_ ## flag) == RES_FILE_ ## flag)
#define res_file_different(rf, flag) (((rf)->rf_diff & RES_FILE_ ## flag) == RES_FILE_ ## flag)

struct res_file {
	unsigned int rf_prio;   /* Priority (0 = highest) */
	char        *rf_lpath;  /* Local path to the file */
	char        *rf_rpath;  /* Path to desired file */

	uid_t        rf_uid;    /* UID of file owner */
	gid_t        rf_gid;    /* GID of file group owner */
	mode_t       rf_mode;   /* File mode (perms only ATM) */

	sha1         rf_lsha1;  /* Local (actual) checksum */
	sha1         rf_rsha1;  /* Remote (expected) checksum */

	struct stat  rf_stat;   /* stat(2) of local file */
	unsigned int rf_enf;    /* enforce-compliance flags */
	unsigned int rf_diff;   /* out-of-compliance flags */
};

void res_file_init(struct res_file *rf);
void res_file_free(struct res_file *rf);

int res_file_set_uid(struct res_file *rf, uid_t uid);
int res_file_unset_uid(struct res_file *rf);

int res_file_set_gid(struct res_file *rf, gid_t gid);
int res_file_unset_gid(struct res_file *rf);

int res_file_set_mode(struct res_file *rf, mode_t mode);
int res_file_unset_mode(struct res_file *rf);

int res_file_set_source(struct res_file *rf, const char *path);
int res_file_unset_source(struct res_file *rf);

int res_file_set_sha1(struct res_file *rf, sha1 *checksum);
int res_file_unset_sha1(struct res_file *rf);

void res_file_merge(struct res_file *rf1, struct res_file *rf2);

int res_file_stat(struct res_file *rf);
int res_file_remediate(struct res_file *rf);
void res_file_dump(struct res_file *rf);

#endif
