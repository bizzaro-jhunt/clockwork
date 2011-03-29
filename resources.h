#ifndef RESOURCES_H
#define RESOURCES_H

#include "clockwork.h"

#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <shadow.h>

#include "resource.h"
#include "report.h"
#include "list.h"
#include "stringlist.h"
#include "hash.h"

#define RES_USER_ABSENT   0x80000000

/** No fields to enforce / No fields are different. */
#define RES_USER_NONE     0x0000
/** Enforce username / username is different. */
#define RES_USER_NAME     0x0001
/** Enforce password / password is different. */
#define RES_USER_PASSWD   0x0002
/** Enforce UID / UID is different. */
#define RES_USER_UID      0x0004
/** Enforce primary group ID / GID is different. */
#define RES_USER_GID      0x0008
/** Enforce GECOS value / GECOS is different. */
#define RES_USER_GECOS    0x0010
/** Enforce home directory value / home directory is different. */
#define RES_USER_DIR      0x0020
/** Enforce user login shell / login shell is different. */
#define RES_USER_SHELL    0x0040
/** Enforce creation of user's home directory / home directory does not exist. */
#define RES_USER_MKHOME   0x0080
/** Enforce minimum password age / minimum password age is different. */
#define RES_USER_PWMIN    0x0100
/** Enforce maximum password age / maximum password age is different. */
#define RES_USER_PWMAX    0x0200
/** Enforce password warning period / password warning period is different. */
#define RES_USER_PWWARN   0x0400
/** Enforce password inactivity period / password inactivity period is different. */
#define RES_USER_INACT    0x0800
/** Enforce account expiration date / account expiration date is different. */
#define RES_USER_EXPIRE   0x1000
/** Enforce account lockout status / account lockout status is different. */
#define RES_USER_LOCK     0x2000

/**
  A system user resource.
 */
struct res_user {
	/** Unique Identifier, starting with "res_user:" */
	char *key;

	/** Username */
	char *ru_name;

	/** Encrypted password, for /etc/shadow */
	char *ru_passwd;

	/** Numeric User ID. */
	uid_t ru_uid;

	/** Numeric Group ID (primary) */
	gid_t ru_gid;

	/** Comment (GECOS) field */
	char *ru_gecos;

	/** Home directory */
	char *ru_dir;

	/** Path to shell */
	char *ru_shell;

	/** Whether or not to make the user's home directory.
	    1 = make the directory; 0 = don't. */
	unsigned char ru_mkhome;

	/** Whether or not this account is locked.
	    1 = locked; 0 = unlocked (active). */
	unsigned char ru_lock;

	/* These members match struct spwd; cf. getspnam(3) */

	/** Minimum days between password changes */
	long ru_pwmin;

	/** Maximum password age (in days) */
	long ru_pwmax;

	/** Number of days before password change to warn user */
	long ru_pwwarn;

	/** Number of days after password expiration to disable the account. */
	long ru_inact;

	/** When the account expires (days since 1/1/70) */
	long ru_expire;

	/** Pointer to an entry in a pwdb */
	struct passwd *ru_pw;

	/** Pointer to an entry in an spdb */
	struct spwd *ru_sp;

	/** Path to skeleton dir (for ru_mkhome) */
	char *ru_skel;

	/** Enforcement flags; see RES_USER_* constants. */
	unsigned int enforced;

	/** Compliance failure flags; see RES_USER_* constants. */
	unsigned int different;

	/** Used in policy node lists. */
	struct list res;
};

NEW_RESOURCE(user);


/** Enforce absence of group / group is present . */
#define RES_GROUP_ABSENT   0x80000000

/** No fields to enforce / No fields are different. */
#define RES_GROUP_NONE     0x00
/** Enforce group name / group name is different. */
#define RES_GROUP_NAME     0x01
/** Enforce group access password / password is different. */
#define RES_GROUP_PASSWD   0x02
/** Enforce group ID / GID is different. */
#define RES_GROUP_GID      0x04
/** Enforce group member roster / member roster is different. */
#define RES_GROUP_MEMBERS  0x08
/** Enforce group admin roster / admin roster is different. */
#define RES_GROUP_ADMINS   0x10

/**
  A system group resource.
 */
struct res_group {
	/** Unique identifier, starting with "res_group:" */
	char *key;

	/** Group name */
	char *rg_name;

	/** Encrypted group password (used for ad-hoc member access. */
	char *rg_passwd;

	/** Numeric Group ID */
	gid_t rg_gid;

	/** Users that should belong to this group; list of usernames. */
	stringlist *rg_mem_add;
	/** Users that cannot belong to this group; list of usernames. */
	stringlist *rg_mem_rm;

	/** Users that should manage this group; list of usernames. */
	stringlist *rg_adm_add;
	/** Users that cannot manage this group; list of usernames. */
	stringlist *rg_adm_rm;

	/** Copy of gr_mem, from stat, for remediation */
	stringlist *rg_mem;
	/** Copy of sg_adm, from stat, for remediation */
	stringlist *rg_adm;

	/** Pointer to an entry in a grdb */
	struct group *rg_grp;
	/** Pointer to an entry in an sgdb */
	struct sgrp *rg_sg;

	/** Enforcement flags; see RES_GROUP_* constants. */
	unsigned int enforced;
	/** Compliance failure flags; see RES_GROUP_* constants. */
	unsigned int different;

	/** Used in policy node lists. */
	struct list res;
};

NEW_RESOURCE(group);

/**
  Set enforcement mode for group membership.

  If \a enforce is 1, then group membership will be enforced.  If
  0, the members roster will not be modified.

  This function works in concert with res_group_add_member and
  res_group_remove_member.

  @param  rg         Group resource to update.
  @param  enforce    Enforcement flag; 1 = enforce roster; 0 = don't.

  @returns 0 on success, non-zero on failure.
 */
int res_group_enforce_members(struct res_group *rg, int enforce);

/**
  Add a user to the list of members.

  @note This function implicitly calls res_group_enforce_members,
        to enable member roster enforcement.

  @param  rg      Group resource to update.
  @param  user    Username of a user to add.

  @returns 0 on success, non-zero on failure.
 */
int res_group_add_member(struct res_group *rg, const char *user);

/**
  Remove a user from the list of members.

  This function allows the caller to specify that the \a user should
  not be a member of the group; the user will be removed if it is
  already found to be a member.

  @note This function implicitly calls res_group_enforce_members,
        to enable member roster enforcement.

  @param  rg      Group resource to update.
  @param  user    Username of user to remove.

  @returns 0 on success, non-zero on failure.
 */
int res_group_remove_member(struct res_group *rg, const char *user);

/**
  Set enforcement mode for group administration.

  If \a enforce is 1, then the group admin roster will be enforced.
  If 0, the admin roster will not be modified.

  This function works in concert with res_group_add_admin and
  res_group_remove_admin.

  @param  rg         Group resource to update.
  @param  enforce    Enforcement flag; 1 = enforce roster; 0 = don't.

  @returns 0 on success, non-zero on failure.
 */
int res_group_enforce_admins(struct res_group *rg, int enforce);

/**
  Add a user to the list of admins.

  @note This function implicitly calls res_group_enforce_admins,
        to enable admin roster enforcement.

  @param  rg      Group resource to update.
  @param  user    Username of a user to add.

  @returns 0 on success, non-zero on failure.
 */
int res_group_add_admin(struct res_group *rg, const char *user);

/**
  Remove a user from the list of admins.

  This function allows the caller to specify that the \a user should
  not be a admin of the group; the user will be removed if it is
  already found to be a admin.

  @note This function implicitly calls res_group_enforce_admins,
        to enable admin roster enforcement.

  @param  rg      Group resource to update.
  @param  user    Username of user to remove.

  @returns 0 on success, non-zero on failure.
 */
int res_group_remove_admin(struct res_group *rg, const char *user);



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
	unsigned int enforced;

	/** Compliance failure flags; see RES_FILE_* contstants. */
	unsigned int different;

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



#define RES_PACKAGE_ABSENT   0x80000000
#define RES_PACKAGE_NONE     0x0

struct res_package {
	char *key;
	char *name;
	char *version;

	char *installed;

	unsigned int enforced;
	unsigned int different;
	struct list res;
};

NEW_RESOURCE(package);

#endif
