#ifndef RES_GROUP_H
#define RES_GROUP_H

#include <stdlib.h>
#include <sys/types.h>
#include <grp.h>

#include "report.h"
#include "list.h"
#include "userdb.h"
#include "stringlist.h"
#include "resource.h"

/** @file res_group.h

  Group resources allow Clockwork to handle group account creation,
  membership and administration on target sytems.  It has full support
  for all fields in the standard UNIX /etc/group file, as well as the
  not-so-standard (or ubiquitous) /etc/gshadow file.

 */

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
  Whether or not \a field is enforced on \a rg.

  @param  rg       Group resource to check.
  @param  field    Part of a RES_GROUP_* constant, without the
                   RES_GROUP_ prefix.
  @returns non-zero if \a field is enforced; zero if not.
 */
#define res_group_enforced(rg, field)  (((rg)->rg_enf  & RES_GROUP_ ## field) == RES_GROUP_ ## field)

/**
  Whether or not \a field differs from the prescribed value in \a rg.

  @param  rg       Group resource to check.
  @param  field    Part of a RES_GROUP_* constant, without the
                   RES_GROUP_ prefix.
  @returns non-zero if \a field differs; zero if not.
 */
#define res_group_different(rg, field) (((rg)->rg_diff & RES_GROUP_ ## field) == RES_GROUP_ ## field)

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
	unsigned int rg_enf;
	/** Compliance failure flags; see RES_GROUP_* constants. */
	unsigned int rg_diff;

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

#if 0
/**
  Check the local /etc/group and /etc/gshadow databases to determine
  the current state of the local group account this resource represents.

  @param  rg      Group resource to query.
  @param  grdb    Group database structure.
  @param  sgdb    GShadow database structure.

  @returns 0 on success, non-zero on failure.
 */
int res_group_stat(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb);

/**
  Modify the entries in /etc/group and /etc/gshadow to bring the local
  group account into compliance with this resource definition.

  @note It is the caller's responsibility to call res_group_stat
        prior to this function.

  This function does not actually commit the changes made to the \a grdb
  and \a sgdb function parameters.  This allows a caller to remediate
  several group accounts, by re-using the same two grdb and sgdb structures,
  and commit all changes when finished.

  @param  rg      Group resource to use for remediation.
  @param  dryrun  Don't remediate, just print what would be done.
  @param  grdb    Group database structure to update.
  @param  sgdb    GShadow database structure to update.

  @returns a pointer to a struct report describing actions taken,
           or NULL on internal failure (i.e. malloc issues)
 */
struct report* res_group_remediate(struct res_group *rg, int dryrun, struct grdb *grdb, struct sgdb *sgdb);

/**
  Determine whether or not \a packed is a packed representation
  of a res_group structure.

  @param  packed    Packed string to check.

  @returns 0 if \a packed represents a res_group structure, and
           non-zero if not.
 */
int res_group_is_pack(const char *packed);

/**
  Serialize a group resource into a string representation.

  The string returned by this function should be freed by the caller,
  using the standard free(3) standard library function.

  The final serialized form of a res_group satisfies the following
  requirements:

  -# Can be passed to res_group_is_pack to get a successful return value.
  -# Can be passed to res_group_unpack to get the original res_group structure back.

  @param  rg    Group resource to serialize.

  @returns a heap-allocated string containing the packed representation
           of the given res_group structure.
 */
char *res_group_pack(struct res_group *rg);

/**
  Unserialize a group resource from its string representation.

  @note The pointer returned by this function should be passed to
        res_group_free to de-allocate and release its memory resources.

  @param  packed    Packed (serialized) res_group representation.

  @returns a heap-allocated res_group structure, based on the information
           found in the \a packed string.
 */
struct res_group* res_group_unpack(const char *packed);
#endif

#endif
