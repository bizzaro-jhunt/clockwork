#ifndef RES_FILE_H
#define RES_FILE_H

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "list.h"
#include "sha1.h"

/** @file res_file.h

  File resources are used to model the attributes of filesystem
  entries, like regular files and directories.  Since many aspects
  of UNIX system administration involve modifying the contents,
  premissions and ownership of files and directories, res_file
  is a very strong and useful resource types.

  The following "attributes" of a file / directory can be modeled
  through res_file:

  -# Ownership, both user and group.
  -# Permissions, including the "higher-order" bits like setuid / setgid.
  -# Contents, checked by SHA1 checksums

  It is also possible to ensure that a file does not exist on the
  target system.  For example, when migrating from one tool to another,
  an administrator may want to thoroughly clean up the /etc directories
  to remove prior configuration.

  @note Currently, extended attributes (xattrs) and file access control
        lists (facl) are not supported.  Patches, as always, are welcome.

 */

/** Enforce absence of file / file is absent. */
#define RES_FILE_ABSENT   0x80000000

/** No fields to enforce /  No fields are different. */
#define RES_FILE_NONE     0x00
/** Enforce UID / UID is different. */
#define RES_FILE_UID      0x01
/** Enforce GID / GID is different. */
#define RES_FILE_GID      0x02
/** Enforce file mode / file mode is different. */
#define RES_FILE_MODE     0x04
/** Enforce file contents / file contents are different. */
#define RES_FILE_SHA1     0x08

/**
  Whether or not \a field is enforced on \a rf.

  @param  rf       File resource to check.
  @param  field    Part of a RES_FILE_* constant, without the
                   RES_FILE_ prefix.
  @returns non-zero if \a field is enforced; zero if not.
 */
#define res_file_enforced(rf, field)  (((rf)->rf_enf  & RES_FILE_ ## field) == RES_FILE_ ## field)

/**
  Whether or not \a field differs from the prescribed value in \a rf.

  @param  rf       File resource to check.
  @param  field    Part of a RES_FILE_* constant, without the
                   RES_FILE_ prefix.
  @returns non-zero if \a field differs; zero if not.
 */
#define res_file_different(rf, field) (((rf)->rf_diff & RES_FILE_ ## field) == RES_FILE_ ## field)

/**
  A filesystem resource (either a file or a directory).
 */
struct res_file {
	/** Unique identifier, starting with "res_file:" */
	char *key;

	/** Local path (client-side) to the file. */
	char *rf_lpath;

	/** Remote path (server-side) to the desired file. */
	char *rf_rpath;

	/** UID of the files user owner. */
	uid_t rf_uid;
	/** GID of the file group owner. */
	gid_t rf_gid;

	/** File mode (permissions) */
	mode_t rf_mode;

	/** Local (client-side) checksum of file contents. */
	sha1 rf_lsha1;
	/** Remote (server-side) checksum of file contents. */
	sha1 rf_rsha1;

	/** stat(2) of the local file; used during remediation. */
	struct stat rf_stat;

	/** File existence flag; 1 = file exists; 0 = does not exist. */
	short rf_exists;

	/** Enforcements flags; see RES_FILE_* constants. */
	unsigned int rf_enf;

	/** Compliance failure flags; see RES_FILE_* contstants. */
	unsigned int rf_diff;

	/** Used in policy node lists. */
	struct list res;
};

/**
  Allocate and initialize a new res_file structure.

  The pointer returned must be freed by a call to res_file_free.

  @param  key    Key for new file resource.

  @returns a pointer to a heap-allocated res_file structure, with all
           members properly initialized to sane default values, or
           NULL on failure.
 */
struct res_file* res_file_new(const char *key);

/**
  Free a dynamically-allocated res_file structure.

  @param  rf    res_file structure to free.
 */
void res_file_free(struct res_file *rf);

/**
  General purpose attribute setter.

  This function is used by the spec parser routines to properly set
  attributes on a file resource.  It calls the other res_file_set_*
  functions, based on the value of \a name.

  @param  rf       File resource to set attribute on.
  @param  name     Name of attribute to set.
  @param  value    Value to set (from parser).

  @returns 0 on success, non-zero on failure.
 */
int res_file_setattr(struct res_file *rf, const char *name, const char *value);

/**
  Set the presence disposition of this file resource.

  If \a presence is passed as 1, then the file represented will be
  created and updated if missing.  If 0, then the absence of the
  file will be enforced, and it will be deleted if found.

  @param  rf          File resource to update.
  @param  presence    Presence flag (1 = present; 0 = absent).

  @returns 0 on success, non-zero on failure.
 */
int res_file_set_presence(struct res_file *rf, int presence);

/**
  Set file ownership (user).

  @param  rf     File resource to update.
  @param  uid    UNIX user id of the file's desired owner.

  @returns 0 on success, non-zero on failure.
 */
int res_file_set_uid(struct res_file *rf, uid_t uid);

/**
  Set file ownership (group).

  @param  rf     File resource to update.
  @param  gid    UNIX group id of the file's desired group.

  @returns 0 on success, non-zero on failure.
 */
int res_file_set_gid(struct res_file *rf, gid_t gid);

/**
  Set file mode / permissions.

  @param  rf      File resource to update.
  @param  mode    UNIX mode value (o.e. 0644 for rwxrw-rw-).

  @returns 0 on success, non-zero on failure.
 */
int res_file_set_mode(struct res_file *rf, mode_t mode);

/**
  Set local file path.

  @param  rf      File resource to update.
  @param  path    Absolute path on the target system.

  @returns 0 on success, non-zero on failure.
 */
int res_file_set_path(struct res_file *rf, const char *path);

/**
  Set remote file path (source).

  @param  rf      File resource to update.
  @param  path    Absolute path on policy master.

  @returns 0 on success, non-zero on failure.
 */
int res_file_set_source(struct res_file *rf, const char *path);

/**
  Query the local filesystem and and populate the res_file::rf_stat
  member with information necessary for remediation.

  @param  rf    File resource to stat.

  @returns 0 on success, non-zero on failure.
 */
int res_file_stat(struct res_file *rf);

/**
  Bring the local file (client-side) in-line with the enforced
  attributes in a res_file structure.

  @note It is the caller's responsibility to call res_file_stat
        prior to this function.

  @param  rf    File resource to use for remediation

  @returns 0 on success, non-zero on failure.
 */
int res_file_remediate(struct res_file *rf);

/**
  Determine whether or not \a packed is a packed representation
  of a res_file structure.

  @param  packed    Packed string to check.

  @returns 0 if \a packed represents a res_file structure, and
           non-zero if not.
 */
int res_file_is_pack(const char *packed);

/**
  Serialize a file resource into a string representation.

  The string returned by this function should be freed by the caller,
  using the standard free(3) standard library function.

  The final serialized form of a res_file satisfies the following
  requirements:

  -# Can be passed to res_file_is_pack to get a successful return value.
  -# Can be passed to res_file_unpack to get the original res_file structure back.

  @param  rf    File resource to serialize.

  @returns a heap-allocated string containing the packed representation
           of the given res_file structure.
 */
char* res_file_pack(struct res_file *rf);

/**
  Unserialize a file resource from its string representation.

  @note The pointer returned by this function should be passed to
        res_file_free to de-allocate and release its memory resources.

  @param  packed    Packed (serialized) res_file representation.

  @returns a heap-allocated res_file structure, based on the information
           found in the \a packed string.
 */
struct res_file* res_file_unpack(const char *packed);

#endif
