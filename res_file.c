#include "res_file.h"

static int _res_file_diff(struct res_file *rf);
static int _res_file_set_sha1_and_source(struct res_file *rf, const sha1 *cksum, const char *path);

/*****************************************************************/

/*
 * Calculate the difference between the expected file attributes
 * in a res_file structure, and the actual file attributes from
 * the filesystem.
 */
static int _res_file_diff(struct res_file *rf)
{
	assert(rf);
	assert(rf->rf_stat);

	rf->rf_diff = RES_FILE_NONE;

	if (res_file_enforced(rf, UID) && rf->rf_uid != rf->rf_stat->st_uid) {
		rf->rf_diff |= RES_FILE_UID;
	}
	if (res_file_enforced(rf, GID) && rf->rf_gid != rf->rf_stat->st_gid) {
		rf->rf_diff |= RES_FILE_GID;
	}
	if (res_file_enforced(rf, MODE) && (rf->rf_stat->st_mode & rf->rf_mode) != rf->rf_mode) {
		rf->rf_diff |= RES_FILE_MODE;
	}
	if (res_file_enforced(rf, SHA1) && memcmp(rf->rf_rsha1.raw, rf->rf_lsha1.raw, SHA1_DIGEST_SIZE) != 0) {
		rf->rf_diff |= RES_FILE_SHA1;
	}

	return 0;
}

static int _res_file_set_sha1_and_source(struct res_file *rf, const sha1 *cksum, const char *path)
{
	assert(rf);

	size_t len = strlen(path) + 1;

	if (rf->rf_rpath) {
		free(rf->rf_rpath);
	}
	rf->rf_rpath = malloc(len);
	if (!rf->rf_rpath) { return -1; }

	if (!memcpy(rf->rf_rpath, path, len)) { return -1; }
	if (!memcpy(&(rf->rf_rsha1), cksum, sizeof(sha1))) { return -1; }

	rf->rf_enf |= RES_FILE_SHA1;

	return 0;
}

/*****************************************************************/

void res_file_init(struct res_file *rf)
{
	assert(rf);
	rf->rf_enf = 0;
	rf->rf_diff = 0;
	rf->rf_stat = NULL;

	rf->rf_lpath = NULL;
	rf->rf_rpath = NULL;

	sha1_init(&(rf->rf_lsha1));
	sha1_init(&(rf->rf_rsha1));
}

/*
 * Set an enforcing UID for a res_file
 */
int res_file_set_uid(struct res_file *rf, uid_t uid)
{
	assert(rf);
	rf->rf_uid = uid;
	rf->rf_enf |= RES_FILE_UID;

	return 0;
}

/*
 * Stop enforcing a UID for a res_file
 */
int res_file_unset_uid(struct res_file *rf)
{
	assert(rf);
	rf->rf_enf ^= RES_FILE_UID;

	return 0;
}

/*
 * Set an enforcing GID for a res_file
 */
int res_file_set_gid(struct res_file *rf, gid_t gid)
{
	assert(rf);
	rf->rf_gid = gid;
	rf->rf_enf |= RES_FILE_GID;

	return 0;
}

/*
 * Stop enforcing a GID for a res_file
 */
int res_file_unset_gid(struct res_file *rf)
{
	assert(rf);
	rf->rf_enf ^= RES_FILE_GID;

	return 0;
}

/*
 * Set an enforcing permissions mode for a res_file
 */
int res_file_set_mode(struct res_file *rf, mode_t mode)
{
	assert(rf);
	rf->rf_mode = mode;
	rf->rf_enf |= RES_FILE_MODE;

	return 0;
}

/*
 * Stop enforcing permissions for a res_file
 */
int res_file_unset_mode(struct res_file *rf)
{
	assert(rf);
	rf->rf_enf ^= RES_FILE_MODE;

	return 0;
}

int res_file_set_source(struct res_file *rf, const char *file)
{
	assert(rf);
	size_t len = strlen(file) + 1;

	rf->rf_rpath = malloc(len);
	if (!rf->rf_rpath) { return -1; }

	strncpy(rf->rf_rpath, file, len);
	if (sha1_file(rf->rf_rpath, &(rf->rf_rsha1)) == -1) { return -1; }

	rf->rf_enf |= RES_FILE_SHA1;

	return 0;
}

int res_file_unset_source(struct res_file *rf)
{
	assert(rf);
	if (rf->rf_rpath) {
		free(rf->rf_rpath);
		rf->rf_rpath = NULL;
	}

	sha1_init(&(rf->rf_rsha1));
	rf->rf_enf ^= RES_FILE_SHA1;

	return 0;
}

/*
 * Set an enforcing SHA1 checksum for a res_file
 */
int res_file_set_sha1(struct res_file *rf, sha1 *checksum)
{
	assert(rf);
	assert(checksum);
	memcpy(rf->rf_rsha1.raw, checksum->raw, SHA1_DIGEST_SIZE);
	memcpy(rf->rf_rsha1.hex, checksum->hex, SHA1_HEX_DIGEST_SIZE + 1);
	rf->rf_enf |= RES_FILE_SHA1;

	return 0;
}

/*
 * Stop enforcing a SHA1 checksum for a res_file
 */
int res_file_unset_sha1(struct res_file *rf)
{
	assert(rf);
	rf->rf_enf ^= RES_FILE_SHA1;

	return 0;
}

/*
 * Merge two res_file structures, respecting priority.
 *
 * rf1 is expected to have a higher priority (lower rf_prio value)
 * than rf2; if not, the pointers are swapped locally.
 *
 * If rf1 and rf2 have the same priority value, rf1 takes priority
 * over rf2 (arbitrarily).
 */
void res_file_merge(struct res_file *rf1, struct res_file *rf2)
{
	assert(rf1);
	assert(rf2);

	struct res_file *tmp;

	if (rf1->rf_prio > rf2->rf_prio) {
		/* out-of-order, swap pointers */
		tmp = rf1; rf1 = rf2; rf2 = tmp; tmp = NULL;
	}

	/* rf1 has a priority over rf2 */
	assert(rf1->rf_prio <= rf2->rf_prio);

	if ( res_file_enforced(rf2, UID) &&
	    !res_file_enforced(rf1, UID)) {
		printf("Overriding UID of rf1 with value from rf2\n");
		res_file_set_uid(rf1, rf2->rf_uid);
	}

	if ( res_file_enforced(rf2, GID) &&
	    !res_file_enforced(rf1, GID)) {
		printf("Overriding GID of rf1 with value from rf2\n");
		res_file_set_gid(rf1, rf2->rf_gid);
	}

	if ( res_file_enforced(rf2, MODE) &&
	    !res_file_enforced(rf1, MODE)) {
		printf("Overriding MODE of rf1 with value from rf2\n");
		res_file_set_mode(rf1, rf2->rf_mode);
	}

	if ( res_file_enforced(rf2, SHA1) &&
	    !res_file_enforced(rf1, SHA1)) {
		printf("Overriding SHA1 of rf1 with value from rf2\n");
		_res_file_set_sha1_and_source(rf1, &(rf2->rf_rsha1), rf2->rf_rpath);
	}
}

/*
 * Fill in the local details of res_file structure,
 * including invoking stat(2)
 */
int res_file_stat(struct res_file *rf)
{
	assert(rf);
	assert(rf->rf_lpath);
	assert(!rf->rf_stat);

	rf->rf_stat = calloc(1, sizeof(struct stat));
	if (!rf->rf_stat) {
		return -1; // out of memory?
	}

	if (stat(rf->rf_lpath, rf->rf_stat) == -1) {
		free(rf->rf_stat);
		rf->rf_stat = NULL;

		return -1;
	}

	/* only generate sha1 checksums if necessary */
	if (res_file_enforced(rf, SHA1)) {
		if (sha1_file(rf->rf_lpath, &(rf->rf_lsha1)) == -1) {
			return -1;
		}
	}

	return _res_file_diff(rf);
}

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
