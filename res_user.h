#ifndef RES_USER_H
#define RES_USER_H

#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>

#include "report.h"
#include "list.h"
#include "resource.h"

/** @file res_user.h

  User resources allow Clockwork to handle user account creation,
  deactivation and deletion on target systems.  It has full support
  for all fields in the standard UNIX /etc/passwd and /etc/shadow
  files, including password aging information.

  Valid Attributes:

  password,pwhash  - Encrypted account password (for /etc/shadow)
  uid - Numeric user ID
  gid  - Primary group (numeric) ID
  gecos,comment - Comment field
  home - Home directory path
  shell - Login shell
  makehome,skeleton - Force creation of home directory (using the given skel)
  pwmin - Password minimum age
  pwmax - Password maximum age
  pwwarn - Password warning period
	The password warning period is the number of days before a password
	is going to expire (according to pwmax) when the user will receive
	warnings about the impending expiration.

	For example, suppose a pwmax value of 30 days and a pwwarn value of
	12 days.  If a user changes their password on March 2nd, the user
	will start receiving warnings to change their password 18 days later,
	on March 20th.
  inact - Password inactivity period
	The password inactivity period is the number of days after a password
	has expired (pwmax) during which the password will still be accepted,
	allowing users with expired passwords to log in and change them.

	After the password inactivity period has ended for an account, it will
	be locked.  For example, with a pwmax of 20 and a password inactivity
	period of 10 days, a user's account will be locked out 30 days after
	they change their password.
  expiry - Date of account expiration (days since Jan 1 1970)
  locked (yes/no)
	If \a locked is 1, the account will be locked, effectively disallowing
	login access by rendering the password hash impossible to generate.
	According to convention, this is done by pre-pending an exclamation
	point (!) to the encrypted password hash; in this way, the original
	password is recoverable, but the account is unusable.
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

NEW_RESOURCE(user);

#endif
