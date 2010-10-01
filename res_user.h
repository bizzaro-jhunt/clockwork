#ifndef RES_USER_H
#define RES_USER_H

#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>

#include "userdb.h"

#define RES_USER_NONE          0
#define RES_USER_NAME    1 <<  0
#define RES_USER_PASSWD  1 <<  1
#define RES_USER_UID     1 <<  2
#define RES_USER_GID     1 <<  3
#define RES_USER_GECOS   1 <<  4
#define RES_USER_DIR     1 <<  5
#define RES_USER_SHELL   1 <<  6
#define RES_USER_MKHOME  1 <<  7
#define RES_USER_PWMIN   1 <<  8
#define RES_USER_PWMAX   1 <<  9
#define RES_USER_PWWARN  1 << 10
#define RES_USER_INACT   1 << 11
#define RES_USER_EXPIRE  1 << 12
#define RES_USER_LOCK    1 << 13

#define res_user_enforced(ru, flag)  (((ru)->ru_enf  & RES_USER_ ## flag) == RES_USER_ ## flag)
#define res_user_different(ru, flag) (((ru)->ru_diff & RES_USER_ ## flag) == RES_USER_ ## flag)

struct res_user {
	char          *ru_name;    /* Username */
	char          *ru_passwd;  /* Encrypted Password */

	uid_t          ru_uid;     /* Numeric User ID */
	gid_t          ru_gid;     /* Numeric Group ID (primary) */

	char          *ru_gecos;   /* Comment (GECOS) field */
	char          *ru_dir;     /* Home directory */
	char          *ru_shell;   /* Path to shell */

	unsigned char  ru_mkhome;  /* 1 - make home directory; 0 - dont */
	unsigned char  ru_lock;    /* 1 - lock account; 0 - unlock */

	/* These members match struct spwd; cf. getspnam(3) */
	long           ru_pwmin;   /* minimum days between password changes */
	long           ru_pwmax;   /* maximum password age (in days) */
	long           ru_pwwarn;  /* days to warn user about impending change */
	long           ru_inact;   /* disable account (days after pw expires */
	long           ru_expire;  /* account expiration (days since 1/1/70) */

	struct passwd *ru_pw;      /* pointer to an entry in pwdb */
	struct spwd   *ru_sp;      /* pointer to an entry in spdb */

	char          *ru_skel;    /* Path to skeleton dir (for ru_mkhome) */

	unsigned int   ru_prio;
	unsigned int   ru_enf;     /* enforce-compliance flags */
	unsigned int   ru_diff;    /* out-of-compliance flags */
};

void res_user_init(struct res_user *ru);
void res_user_free(struct res_user *ru);

int res_user_set_name(struct res_user *ru, const char *name);
int res_user_unset_name(struct res_user *ru);

int res_user_set_passwd(struct res_user *ru, const char *passwd);
int res_user_unset_passwd(struct res_user *ru);

int res_user_set_uid(struct res_user *ru, uid_t uid);
int res_user_unset_uid(struct res_user *ru);

int res_user_set_gid(struct res_user *ru, gid_t gid);
int res_user_unset_gid(struct res_user *ru);

int res_user_set_gecos(struct res_user *ru, const char *gecos);
int res_user_unset_gecos(struct res_user *ru);

int res_user_set_dir(struct res_user *ru, const char *path);
int res_user_unset_dir(struct res_user *ru);

int res_user_set_shell(struct res_user *ru, const char *shell);
int res_user_unset_shell(struct res_user *ru);

int res_user_set_makehome(struct res_user *ru, unsigned char mkhome, const char *skel);
int res_user_unset_makehome(struct res_user *ru);

int res_user_set_pwmin(struct res_user *ru, long days);
int res_user_unset_pwmin(struct res_user *ru);

int res_user_set_pwmax(struct res_user *ru, long days);
int res_user_unset_pwmax(struct res_user *ru);

int res_user_set_pwwarn(struct res_user *ru, long days);
int res_user_unset_pwwarn(struct res_user *ru);

int res_user_set_inact(struct res_user *ru, long days);
int res_user_unset_inact(struct res_user *ru);

int res_user_set_expire(struct res_user *ru, long days);
int res_user_unset_expire(struct res_user *ru);

int res_user_set_lock(struct res_user *ru, unsigned char locked);
int res_user_unset_lock(struct res_user *ru);

void res_user_merge(struct res_user *ru1, struct res_user *ru2);

int res_user_stat(struct res_user *ru, struct pwdb *pwdb, struct spdb *spdb);
int res_user_remediate(struct res_user *ru, struct pwdb *pwdb, struct spdb *spdb);

#endif
