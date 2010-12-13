#include <assert.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "resource.h"
#include "res_file.h"
#include "pack.h"
#include "mem.h"

#define RF_FD2FD_CHUNKSIZE 16384

#define RES_FILE_PACK_PREFIX "res_file::"
#define RES_FILE_PACK_OFFSET 10
/* Pack format for res_file structure:
     L - rf_enf
     a - rf_lpath
     a - rf_rpath
     L - rf_uid
     L - rf_gid
     L - rf_mode
 */
#define RES_FILE_PACK_FORMAT "LaaLLL"


static int _res_file_fd2fd(int dest, int src);
static int _res_file_diff(struct res_file *rf);

/*****************************************************************/

static int _res_file_fd2fd(int dest, int src)
{
	char buf[RF_FD2FD_CHUNKSIZE];
	ssize_t nread;

	while ( (nread = read(src, buf, RF_FD2FD_CHUNKSIZE)) > 0) {
		if (write(dest, buf, nread) != nread) { return -1; }
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

struct res_file* res_file_new(const char *key)
{
	struct res_file *rf;

	rf = malloc(sizeof(struct res_file));
	if (!rf) {
		return NULL;
	}

	list_init(&rf->res);

	rf->rf_enf = 0;
	rf->rf_diff = 0;
	memset(&rf->rf_stat, 0, sizeof(struct stat));
	rf->rf_exists = 0;

	rf->rf_uid = 0;
	rf->rf_gid = 0;
	rf->rf_mode = 0600; /* sane default... */

	rf->rf_lpath = NULL;
	rf->rf_rpath = NULL;

	sha1_init(&(rf->rf_lsha1));
	sha1_init(&(rf->rf_rsha1));

	if (key) {
		res_file_set_path(rf, key);
		rf->key = resource_key("res_file", key);
	} else {
		rf->key = NULL;
	}

	return rf;
}

void res_file_free(struct res_file *rf)
{
	if (rf) {
		list_del(&rf->res);

		free(rf->rf_rpath);
		free(rf->rf_lpath);

		free(rf->key);
	}

	free(rf);
}

int res_file_setattr(struct res_file *rf, const char *name, const char *value)
{
	if (strcmp(name, "owner") == 0) {
		return res_file_set_uid(rf, 0); /* FIXME: hard-coded UID */
	} else if (strcmp(name, "group") == 0) {
		return res_file_set_gid(rf, 0); /* FIXME: hard-coded GID */
	} else if (strcmp(name, "lpath") == 0) {
		return res_file_set_path(rf, value);
	} else if (strcmp(name, "mode") == 0) {
		return res_file_set_mode(rf, strtoll(value, NULL, 0));
	} else if (strcmp(name, "source") == 0) {
		return res_file_set_source(rf, value);
	} else if (strcmp(name, "present") == 0) {
		return res_file_set_presence(rf, strcmp(value, "no"));
	}

	return -1;
}

int res_file_set_presence(struct res_file *rf, int presence)
{
	assert(rf);

	if (presence) {
		rf->rf_enf ^= RES_FILE_ABSENT;
	} else {
		rf->rf_enf |= RES_FILE_ABSENT;
	}

	return 0;
}

int res_file_set_uid(struct res_file *rf, uid_t uid)
{
	assert(rf);
	rf->rf_uid = uid;
	rf->rf_enf |= RES_FILE_UID;

	return 0;
}

int res_file_set_gid(struct res_file *rf, gid_t gid)
{
	assert(rf);
	rf->rf_gid = gid;
	rf->rf_enf |= RES_FILE_GID;

	return 0;
}

int res_file_set_mode(struct res_file *rf, mode_t mode)
{
	assert(rf);
	rf->rf_mode = mode & 07777; /* mask off non-perm bits */
	rf->rf_enf |= RES_FILE_MODE;

	return 0;
}

int res_file_set_path(struct res_file *rf, const char *file)
{
	assert(rf);
	size_t len = strlen(file) + 1;

	xfree(rf->rf_lpath);
	rf->rf_lpath = malloc(len);
	if (!rf->rf_lpath) { return -1; }
	strncpy(rf->rf_lpath, file, len);

	return 0;
}

int res_file_set_source(struct res_file *rf, const char *file)
{
	assert(rf);
	size_t len = strlen(file) + 1;

	rf->rf_enf |= RES_FILE_SHA1;

	xfree(rf->rf_rpath);
	rf->rf_rpath = malloc(len);
	if (!rf->rf_rpath) { return -1; }

	strncpy(rf->rf_rpath, file, len);
	if (sha1_file(rf->rf_rpath, &(rf->rf_rsha1)) == -1) { return -1; }

	return 0;
}

/*
 * Fill in the local details of res_file structure,
 * including invoking stat(2)
 */
int res_file_stat(struct res_file *rf)
{
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

int res_file_remediate(struct res_file *rf)
{
	assert(rf);

	/* UID and GID to chown to */
	uid_t uid = (res_file_enforced(rf, UID) ? rf->rf_uid : rf->rf_stat.st_uid);
	gid_t gid = (res_file_enforced(rf, GID) ? rf->rf_gid : rf->rf_stat.st_gid);
	int local_fd; int remote_fd;

	/* Remove the file */
	if (res_file_enforced(rf, ABSENT)) {
		if (rf->rf_exists == 1) {
			return unlink(rf->rf_lpath);
		}

		return 0;
	}

	if (!rf->rf_exists) {
		local_fd = creat(rf->rf_lpath, rf->rf_mode);
		if (local_fd < 0) {
			return local_fd;
		}

		/* No need to chmod the file again */
		rf->rf_enf ^= RES_FILE_MODE;
	}

	if (res_file_different(rf, SHA1)) {
		assert(rf->rf_lpath);
		assert(rf->rf_rpath);

		local_fd = open(rf->rf_lpath, O_CREAT | O_RDWR | O_TRUNC, rf->rf_mode);
		if (local_fd == -1) { return -1; }

		remote_fd = open(rf->rf_rpath, O_RDONLY);
		if (remote_fd == -1) { return -1; }

		if (_res_file_fd2fd(local_fd, remote_fd) == -1) {
			return -1;
		}
	}

	if (res_file_different(rf, UID) || res_file_different(rf, GID)) {
		if (chown(rf->rf_lpath, uid, gid) == -1) {
			return -1;
		}
	}

	if (res_file_different(rf, MODE)) {
		if (chmod(rf->rf_lpath, rf->rf_mode) == -1) {
			return -1;
		}
	}

	return 0;
}

int res_file_is_pack(const char *packed)
{
	return strncmp(packed, RES_FILE_PACK_PREFIX, RES_FILE_PACK_OFFSET);
}

char* res_file_pack(struct res_file *rf)
{
	char *packed;
	size_t pack_len;

	pack_len = pack(NULL, 0, RES_FILE_PACK_FORMAT,
		rf->rf_enf,
		rf->rf_lpath, rf->rf_rpath, rf->rf_uid, rf->rf_gid, rf->rf_mode);

	packed = malloc(pack_len + RES_FILE_PACK_OFFSET);
	strncpy(packed, RES_FILE_PACK_PREFIX, RES_FILE_PACK_OFFSET);

	pack(packed + RES_FILE_PACK_OFFSET, pack_len, RES_FILE_PACK_FORMAT,
		rf->rf_enf,
		rf->rf_lpath, rf->rf_rpath, rf->rf_uid, rf->rf_gid, rf->rf_mode);

	return packed;
}

struct res_file* res_file_unpack(const char *packed)
{
	struct res_file *rf;

	if (strncmp(packed, RES_FILE_PACK_PREFIX, RES_FILE_PACK_OFFSET) != 0) {
		return NULL;
	}

	rf = res_file_new(NULL);
	if (unpack(packed + RES_FILE_PACK_OFFSET, RES_FILE_PACK_FORMAT,
		&rf->rf_enf,
		&rf->rf_lpath, &rf->rf_rpath, &rf->rf_uid, &rf->rf_gid, &rf->rf_mode) != 0) {

		res_file_free(rf);
		return NULL;
	}

	rf->key = resource_key("res_file", rf->rf_lpath);

	return rf;
}

