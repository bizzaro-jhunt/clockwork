#ifndef RES_USER_H
#define RES_USER_H

#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>

#include "report.h"
#include "list.h"
#include "userdb.h"

/** @file res_user.h

  User resources allow Clockwork to handle user account creation,
  deactivation and deletion on target systems.  It has full support
  for all fields in the standard UNIX /etc/passwd and /etc/shadow
  files, including password aging information.

 */

/** Enforce absence of user / user is present. */
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
  Whether or not \a field is enforced on \a ru.

  @param  ru       User resource to check.
  @param  field    Part of a RES_USER_* constant, without the
                   RES_USER_ prefix.
  @returns non-zero if \a field is enforced; zero if not.
 */
#define res_user_enforced(ru, field)  (((ru)->ru_enf  & RES_USER_ ## field) == RES_USER_ ## field)

/**
  Whether or not \a field differs from the prescribed value in \a ru.

  @param  ru       User resource to check.
  @param  field    Part of a RES_USER_* constant, without the
                   RES_USER_ prefix.
  @returns non-zero if \a field differs; zero if not.
 */
#define res_user_different(ru, field) (((ru)->ru_diff & RES_USER_ ## field) == RES_USER_ ## field)

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
	unsigned int ru_enf;

	/** Compliance failure flags; see RES_USER_* constants. */
	unsigned int ru_diff;

	/** Used in policy node lists. */
	struct list res;
};

/**
  Allocate and initialize a new res_user structure.

  The pointer returned must be freed by a call to res_user_free.

  @param  key    Key for new user resource.

  @returns a pointer to a heap-allocated res_user structure, with all
           members properly initialized to sane default values, or
           NULL on failure.
 */
struct res_user* res_user_new(const char *key);

/**
  Free a dynamically-allocated res_user structure.

  @param  ru    res_user structure to free.
 */
void res_user_free(struct res_user *ru);

/**
  General purpose attribute setter.

  This function is used by the spec parser routines to properly set
  attributes on a user resource.  It calls the other res_user_set_*
  functions, based on the value of \a name.

  @param  ru       User resource to set attribute on.
  @param  name     Name of the attribute to set.
  @param  value    Value to set (from parser).

  @returns 0 on success, non-zero on failure.
 */
int res_user_setattr(struct res_user *ru, const char *name, const char *value);

/**
  Set the presence disposition of this user resource.

  If \a presence is passed as 1, then the user represented will be
  updated, and created if missing.  If 0, then the absence of the user
  will be enforced, and it will be removed if found.

  @param  ru          User resource to update.
  @param  presence    Presence flag (1 = present; 0 = absent).

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_presence(struct res_user *ru, int presence);

/**
  Set account username.

  @param  ru      User resource to update.
  @param  name    The username to use.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_name(struct res_user *ru, const char *name);

/**
  Set account password.

  @param  ru        User resource to update.
  @param  passwd    Encrypted password, for insertion into /etc/shadow.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_passwd(struct res_user *ru, const char *passwd);

/**
  Set numeric user ID.

  @param  ru     User resource to update.
  @param  uid    UID for this account.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_uid(struct res_user *ru, uid_t uid);

/**
  Set primary group ID (numeric).

  @param  ru     User resource to update.
  @param  gid    GID for this account's primary group.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_gid(struct res_user *ru, gid_t gid);

/**
  Set the GECOS / Comment field.

  @param  ru       User resource to update.
  @param  gecos    Value of GECOS.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_gecos(struct res_user *ru, const char *gecos);

/**
  Set the user's home directory path.

  @param  ru      User resource to update.
  @param  path    Absolute path of user's home directory.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_dir(struct res_user *ru, const char *path);

/**
  Set the user's login shell.

  @param  ru       User resource to update.
  @param  shell    Absolute path to the shell binary.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_shell(struct res_user *ru, const char *shell);

/**
  Set the "make home" flag, for this account.

  If set to 1, the "make home" flag causes the client to ensure that the
  home directory (see res_user_set_dir) exists.  The optional \a skel
  argument contains the path to a skeleton directory, which houses initial
  files and directories that should be copied into the newly created home
  directory.

  @note If the user's home directory already exists, the \a skel parameter
        is ignored, and the contents of the directory are not modified.

  @param  ru        User resource to update.
  @param  mkhome    Whether or not to make the home directory.
  @param  skel      Path to a skeleton directory for directory creation.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_makehome(struct res_user *ru, unsigned char mkhome, const char *skel);

/**
  Set the account minimum password age.

  The minimum password age is the minimum number of days after changing
  a password before the user is allowed to change their password again.
  The root user is not constrained by this limit.

  This field is stored in /etc/shadow.

  @param  ru      User resource to update.
  @param  days    Value of minimum password age.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_pwmin(struct res_user *ru, long days);

/**
  Sets the account maximum password age.

  The maximum password age is the number of days after changing their
  password that a user will be required to change it again.

  This field is stored in /etc/shadow.

  @param  ru      User resource to update.
  @param  days    Value of maximum password age.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_pwmax(struct res_user *ru, long days);

/**
  Sets the password warning period.

  The password warning period is the number of days before a password
  is going to expire (according to pwmax) when the user will receive
  warnings about the impending expiration.

  For example, suppose a pwmax value of 30 days and a pwwarn value of
  12 days.  If a user changes their password on March 2nd, the user
  will start receiving warnings to change their password 18 days later,
  on March 20th.

  This field is stored in /etc/shadow.

  @param  ru      User resource to update.
  @param  days    Value of password warning period.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_pwwarn(struct res_user *ru, long days);

/**
  Set the password inactivity period.

  The password inactivity period is the number of days after a password
  has expired (pwmax) during which the password will still be accepted,
  allowing users with expired passwords to log in and change them.

  After the password inactivity period has ended for an account, it will
  be locked.  For example, with a pwmax of 20 and a password inactivity
  period of 10 days, a user's account will be locked out 30 days after
  they change their password.

  This field is stored in /etc/shadow.

  @param  ru      User resource to update.
  @param  days    Value for password inactivity period.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_inact(struct res_user *ru, long days);

/**
  Set the date of the account's expiration.

  Account expiration, specified in the number of days since Jan 1 1970,
  identifies an explicit point in time when the user account will be
  forcefully de-activated, without regard for its current password
  expiration.

  This field is stored in /etc/shadow.

  @param  ru      User resource to update.
  @param  days    Value for account expiration date.

  @returns 0 on success, non-zero on failure.
 */
int res_user_set_expire(struct res_user *ru, long days);

/**
  Set the lock status of this user account.

  If \a locked is 1, the account will be locked, effectively disallowing
  login access by rendering the password hash impossible to generate.
  According to convention, this is done by pre-pending an exclamation
  point (!) to the encrypted password hash; in this way, the original
  password is recoverable, but the account is unusable.

  @param  ru        User resource to update.
  @param  locked    Whether or not to lock the account.
                    1 = lock; 0 = don't lock.

  @returns 0 on sucess, non-zero on failure.
 */
int res_user_set_lock(struct res_user *ru, unsigned char locked);

/**
  Check the local /etc/passwd and /etc/shadow databases to determine
  the current state of the local user account this resource represents.

  @param  ru      User resource to query.
  @param  pwdb    Password database structure.
  @param  spdb    Shadow database structure.

  @returns 0 on success, non-zero on failure.
 */
int res_user_stat(struct res_user *ru, struct pwdb *pwdb, struct spdb *spdb);

/**
  Modify the entries in /etc/passwd and /etc/shadow to bring the local
  user account into compliance with the resource definition.

  @note It is the caller's responsibility to call res_user_stat
        prior to this function.

  This function does not actually commit the changes made to the \a pwdb
  and \a spdb function parameters.  This allows a caller to remediate
  several user accounts, by re-using the same two pwdb and spdb structures,
  and commit all changes when finished.

  @param  ru      User resource to use for remediation.
  @param  dryrun  Don't remediate, just print what would be done.
  @param  pwdb    Password database structure to update.
  @param  spdb    Shadow database structure to update.

  @returns a pointer to a struct report describing the actions taken,
           or NULL on internal failure (i.e. malloc issues)
 */
struct report* res_user_remediate(struct res_user *ru, int dryrun, struct pwdb *pwdb, struct spdb *spdb);

/**
  Determine whether or not \a packed is a packed representation
  of a res_user structure.

  @param  packed    Packed string to check.

  @returns 0 if \a packed represents a res_user structure, and
           non-zero if not.
 */
int res_user_is_pack(const char *packed);

/**
  Serialize a user resource into a string representation.

  The string returned by this function should be freed by the caller,
  using the standard free(3) standard library function.

  The final serialized form of a res_user satisfies the following
  requirements:

  -# Can be passed to res_user_is_pack to get a successful return value.
  -# Can be passed to res_user_unpack to get the original res_user structure back.

  @param  ru    User resource to serialize.

  @returns a heap-allocated string containing the packed representation
           of the given res_user structure.
 */
char* res_user_pack(const struct res_user *ru);

/**
  Unserialize a user resource from its string representation.

  @note The pointer returned by this function should be passed to
        res_user_free to de-allocate and release its memory resources.

  @param  packed    Packed (serialized) res_user representation.

  @returns a heap-allocated res_user structure, based on the information
           found in the \a packed string.
 */
struct res_user* res_user_unpack(const char *packed);

#endif
