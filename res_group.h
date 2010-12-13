#ifndef RES_GROUP_H
#define RES_GROUP_H

#include <stdlib.h>
#include <sys/types.h>
#include <grp.h>

#include "list.h"
#include "userdb.h"
#include "stringlist.h"

/** @file res_group.h

  Group resources allow Clockwork to handle group account creation,
  membership and administration on target sytems.  It has full support
  for all fields in the standard UNIX /etc/group file, as well as the
  not-so-standard (or ubiquitous) /etc/gshadow file.

 */

#define RES_GROUP_ABSENT   0x80000000

#define RES_GROUP_NONE     0x00
#define RES_GROUP_NAME     0x01
#define RES_GROUP_PASSWD   0x02
#define RES_GROUP_GID      0x04
#define RES_GROUP_MEMBERS  0x08
#define RES_GROUP_ADMINS   0x10

#define res_group_enforced(rg, flag)  (((rg)->rg_enf  & RES_GROUP_ ## flag) == RES_GROUP_ ## flag)
#define res_group_different(rg, flag) (((rg)->rg_diff & RES_GROUP_ ## flag) == RES_GROUP_ ## flag)

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

/**
  Allocate and initialize a new res_group structure.

  The pointer returned must be freed by a call to res_group_free.

  @param  key    Key for new group resource.

  @returns a pointer to a heap-allocated res_group structure, with all
           members properly initialized to sane default values, or NULL
           on failure.
 */
struct res_group *res_group_new(const char *key);

/**
  Free a dynamically allocated res_group structure.

  @param  rg    res_group structure to free.
 */
void res_group_free(struct res_group *rg);

/**
  General purpose attribute setter.

  This function is used by the spec parser routines to properly set
  attributes on a group resource.  It calls the other res_group_set_*
  functions, based on the value of \a name.

  @param  rg       Group resource to set attribute on.
  @param  name     Name of the attribute to set.
  @param  value    Value to set (from parser).

  @returns 0 on success, non-zero on failure.
 */
int res_group_setattr(struct res_group *rg, const char *name, const char *value);

/**
  Set the presence disposition of this group resource.

  If \a presence is passed as 1, then the group represented will be
  updated, and created if missing.  If 0, then the absence of the group
  will be enforced, and it will be removed if found.

  @param  rg          Group resource to update.
  @param  presence    Presence flag (1 = present; 0 = absent)

  @returns 0 on success, non-zero on failure.
 */
int res_group_set_presence(struct res_group *rg, int presence);

/**
  Set group name.

  @param  rg      Group resource to update.
  @param  name    The group name to use.

  @returns 0 on success, non-zero on falure.
 */
int res_group_set_name(struct res_group *rg, const char *name);

/**
  Set the encrypted group password.

  This password will be used by non-members who wish to gain
  the permissions of this group.  The newgrp(1) utility allows
  users to do this.

  This field is stored in /etc/gshadow.

  @param  rg        Group resource to update.
  @param  passwd    Encrypted password string.

  @returns 0 on success, non-zero on failure.
 */
int res_group_set_passwd(struct res_group *rg, const char *passwd);

/**
  Set the numeric group ID.

  @param  rg     Group resource to update.
  @param  gid    GID for this group.

  @returns 0 on success, non-zero on failure.
 */
int res_group_set_gid(struct res_group *rg, gid_t gid);

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
  @param  grdb    Group database structure to update.
  @param  sgdb    GShadow database structure to update.
 */
int res_group_remediate(struct res_group *rg, struct grdb *grdb, struct sgdb *sgdb);

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
