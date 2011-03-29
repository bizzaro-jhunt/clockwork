#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "res_file.h"
#include "pack.h"

#define RF_FD2FD_CHUNKSIZE 16384

#define RES_FILE_PACK_PREFIX "res_file::"
#define RES_FILE_PACK_OFFSET 10
/* Pack format for res_file structure:
     L - rf_enf
     a - rf_lpath
     a - rf_rsha1
     a - rf_owner
     a - rf_group
     L - rf_mode
 */
#define RES_FILE_PACK_FORMAT "LaaaaL"


static int _res_file_fd2fd(int dest, int src, ssize_t bytes);
static int _res_file_diff(struct res_file *rf);

/*****************************************************************/

static int _res_file_fd2fd(int dest, int src, ssize_t bytes)
{
	char buf[RF_FD2FD_CHUNKSIZE];
	ssize_t nread = 0;

	while (bytes > 0) {
		nread = (RF_FD2FD_CHUNKSIZE > bytes ? bytes : RF_FD2FD_CHUNKSIZE);

		if ((nread = read(src, buf, nread)) <= 0
		 || write(dest, buf, nread) != nread) {
			return -1;
		}

		bytes -= nread;
	}

	return 0;
}

/*
 * Calculate the difference between the expected file attributes
 * in a res_file structure, and the actual file attributes from
 * the filesystem.
 */
static int _res_file_diff(struct res_file *rf)
{
	assert(rf);

	rf->rf_diff = RES_FILE_NONE;

	if (res_file_enforced(rf, UID) && rf->rf_uid != rf->rf_stat.st_uid) {
		rf->rf_diff |= RES_FILE_UID;
	}
	if (res_file_enforced(rf, GID) && rf->rf_gid != rf->rf_stat.st_gid) {
		rf->rf_diff |= RES_FILE_GID;
	}
	if (res_file_enforced(rf, MODE) && (rf->rf_stat.st_mode & 07777) != rf->rf_mode) {
		rf->rf_diff |= RES_FILE_MODE;
	}
	if (res_file_enforced(rf, SHA1) && memcmp(rf->rf_rsha1.raw, rf->rf_lsha1.raw, SHA1_DIGEST_SIZE) != 0) {
		rf->rf_diff |= RES_FILE_SHA1;
	}

	return 0;
}

/*****************************************************************/

void* res_file_new(const char *key)
{
	struct res_file *rf;

	rf = xmalloc(sizeof(struct res_file));
	list_init(&rf->res);

	rf->rf_enf = 0;
	rf->rf_diff = 0;
	memset(&rf->rf_stat, 0, sizeof(struct stat));
	rf->rf_exists = 0;

	rf->rf_owner = NULL;
	rf->rf_uid = 0;

	rf->rf_group = NULL;
	rf->rf_gid = 0;

	rf->rf_mode = 0600; /* sane default... */

	rf->rf_lpath = NULL;
	rf->rf_rpath = NULL;

	sha1_init(&(rf->rf_lsha1), NULL);
	sha1_init(&(rf->rf_rsha1), NULL);

	if (key) {
		res_file_set(rf, "path", key);
		rf->key = string("res_file:%s", key);
	} else {
		rf->key = NULL;
	}

	return rf;
}

void res_file_free(void *res)
{
	struct res_file *rf = (struct res_file*)(res);
	if (rf) {
		list_del(&rf->res);

		free(rf->rf_rpath);
		free(rf->rf_lpath);

		free(rf->key);
	}

	free(rf);
}

const char* res_file_key(const void *res)
{
	const struct res_file *rf = (struct res_file*)(res);
	assert(rf);

	return rf->key;
}

int res_file_norm(void *res) {
	struct res_file *rf = (struct res_file*)(res);
	assert(rf);

	return sha1_file(rf->rf_rpath, &rf->rf_rsha1);
}

int res_file_set(void *res, const char *name, const char *value)
{
	struct res_file *rf = (struct res_file*)(res);
	assert(rf);

	if (strcmp(name, "owner") == 0) {
		free(rf->rf_owner);
		rf->rf_owner = strdup(value);
		rf->rf_enf |= RES_FILE_UID;

	} else if (strcmp(name, "group") == 0) {
		free(rf->rf_group);
		rf->rf_group = strdup(value);
		rf->rf_enf |= RES_FILE_GID;

	} else if (strcmp(name, "mode") == 0) {
		/* Mask off non-permission bits */
		rf->rf_mode = strtoll(value, NULL, 0) & 07777;
		rf->rf_enf |= RES_FILE_MODE;

	} else if (strcmp(name, "source") == 0) {
		free(rf->rf_rpath);
		rf->rf_rpath = strdup(value);
		if (sha1_file(rf->rf_rpath, &rf->rf_rsha1) != 0) {
			return -1;
		}
		rf->rf_enf |= RES_FILE_SHA1;

	} else if (strcmp(name, "path") == 0) {
		free(rf->rf_lpath);
		rf->rf_lpath = strdup(value);

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") != 0) {
			rf->rf_enf ^= RES_FILE_ABSENT;
		} else {
			rf->rf_enf |= RES_FILE_ABSENT;
		}

	} else {
		/* unknown attribute. */
		return -1;
	}

	return 0;
}

/*
 * Fill in the local details of res_file structure,
 * including invoking stat(2)
 */
int res_file_stat(void *res, const struct resource_env *env)
{
	struct res_file *rf = (struct res_file*)(res);

	assert(rf);
	assert(rf->rf_lpath);

	if (stat(rf->rf_lpath, &rf->rf_stat) == -1) { /* new file */
		rf->rf_diff = rf->rf_enf;
		rf->rf_exists = 0;
		return 0;
	}

	rf->rf_exists = 1;

	/* only generate sha1 checksums if necessary */
	if (res_file_enforced(rf, SHA1)) {
		if (sha1_file(rf->rf_lpath, &(rf->rf_lsha1)) == -1) {
			return -1;
		}
	}

	return _res_file_diff(rf);
}

struct report* res_file_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_file *rf = (struct res_file*)(res);
	assert(rf);
	assert(env);

	struct report *report = report_new("File", rf->rf_lpath);
	char *action;
	int new_file = 0;
	int local_fd = -1;

	/* Remove the file */
	if (res_file_enforced(rf, ABSENT)) {
		if (rf->rf_exists == 1) {
			action = string("remove file");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
			} else if (unlink(rf->rf_lpath) == 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
			}
		}

		return report;
	}

	if (!rf->rf_exists) {
		new_file = 1;
		action = string("create file");

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			local_fd = creat(rf->rf_lpath, rf->rf_mode);
			if (local_fd >= 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
				return report;
			}
		}

		rf->rf_diff = rf->rf_enf;
		/* No need to chmod the file again */
		rf->rf_diff ^= RES_FILE_MODE;
	}

	if (res_file_different(rf, SHA1)) {
		assert(rf->rf_lpath);

		action = string("update content from master copy");
		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			if (local_fd < 0) {
				local_fd = open(rf->rf_lpath, O_CREAT | O_RDWR | O_TRUNC, rf->rf_mode);
				if (local_fd == -1) {
					report_action(report, action, ACTION_FAILED);
					return report;
				}
			}

			if (env->file_fd == -1) {
				report_action(report, action, ACTION_FAILED);
				return report;
			}

			if (_res_file_fd2fd(local_fd, env->file_fd, env->file_len) == -1) {
				report_action(report, action, ACTION_FAILED);
				return report;
			}

			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_file_different(rf, UID)) {
		if (new_file) {
			action = string("set owner to %s(%u)", rf->rf_owner, rf->rf_uid);
		} else {
			action = string("change owner from %u to %s(%u)", rf->rf_stat.st_uid, rf->rf_owner, rf->rf_uid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (chown(rf->rf_lpath, rf->rf_uid, -1) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	if (res_file_different(rf, GID)) {
		if (new_file) {
			action = string("set group to %s(%u)", rf->rf_group, rf->rf_gid);
		} else {
			action = string("change group from %u to %s(%u)", rf->rf_stat.st_gid, rf->rf_group, rf->rf_gid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (chown(rf->rf_lpath, -1, rf->rf_gid) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	if (res_file_different(rf, MODE)) {
		if (new_file) {
			action = string("set permissions to %04o", rf->rf_mode);
		} else {
			action = string("change permissions from %04o to %04o", rf->rf_stat.st_mode & 07777, rf->rf_mode);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (chmod(rf->rf_lpath, rf->rf_mode) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	return report;
}

int res_file_is_pack(const char *packed)
{
	assert(packed);
	return strncmp(packed, RES_FILE_PACK_PREFIX, RES_FILE_PACK_OFFSET);
}

char* res_file_pack(const void *res)
{
	const struct res_file *rf = (const struct res_file*)(res);
	assert(rf);

	return pack(RES_FILE_PACK_PREFIX, RES_FILE_PACK_FORMAT,
	            rf->rf_enf,
	            rf->rf_lpath, rf->rf_rsha1.hex, rf->rf_owner, rf->rf_group, rf->rf_mode);
}

void* res_file_unpack(const char *packed)
{
	char *hex = NULL;
	struct res_file *rf = res_file_new(NULL);

	if (unpack(packed, RES_FILE_PACK_PREFIX, RES_FILE_PACK_FORMAT,
		&rf->rf_enf,
		&rf->rf_lpath, &hex, &rf->rf_owner, &rf->rf_group, &rf->rf_mode) != 0) {

		free(hex);
		res_file_free(rf);
		return NULL;
	}

	sha1_init(&rf->rf_rsha1, hex);
	free(hex);

	rf->key = string("res_file:%s", rf->rf_lpath);

	return rf;
}

