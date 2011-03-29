#ifndef RES_FILE_H
#define RES_FILE_H

#include "clockwork.h"
#include <sys/types.h>
#include <sys/stat.h>

#include "list.h"
#include "sha1.h"
#include "hash.h"
#include "report.h"
#include "resource.h"

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

	/** Name of the file user owner. */
	char *rf_owner;

	/** UID of the files user owner. */
	uid_t rf_uid;

	/** Name of the file group owner. */
	char *rf_group;

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

NEW_RESOURCE(file);

/**
  Retrieve an open stdio FILE handle for a file resource.

  This function will open the static file that \a rf represents
  in read-only mode, and return the stdio FILE handle.  It is the
  callers responsibility to fclose the returned FILE handle when
  finished.

  @param  rf     File Resource to "open"

  @returns An open, read-only file handle to the server-side copy of
           the file resource's contents on success, or NULL on failure.
 */
FILE* res_file_io(struct res_file *rf);

#endif
