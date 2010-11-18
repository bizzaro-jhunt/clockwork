#ifndef RES_USER_H
#define RES_USER_H

#include <stdlib.h>
#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>

#include "list.h"
#include "userdb.h"

#define RES_USER_ABSENT   0x80000000

#define RES_USER_NONE     0x0000
#define RES_USER_NAME     0x0001
#define RES_USER_PASSWD   0x0002
#define RES_USER_UID      0x0004
#define RES_USER_GID      0x0008
#define RES_USER_GECOS    0x0010
#define RES_USER_DIR      0x0020
#define RES_USER_SHELL    0x0040
#define RES_USER_MKHOME   0x0080
#define RES_USER_PWMIN    0x0100
#define RES_USER_PWMAX    0x0200
#define RES_USER_PWWARN   0x0400
#define RES_USER_INACT    0x0800
#define RES_USER_EXPIRE   0x1000
#define RES_USER_LOCK     0x2000

#define res_user_enforced(ru, flag)  (((ru)->ru_enf  & RES_USER_ ## flag) == RES_USER_ ## flag)
#define res_user_different(ru, flag) (((ru)->ru_diff & RES_USER_ ## flag) == RES_USER_ ## flag)

struct res_user {
	char          *key;        /* Unique Identifier; starts with "res_user:" */

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

	unsigned int   ru_enf;     /* enforce-compliance flags */
	unsigned int   ru_diff;    /* out-of-compliance flags */

	struct list    res;        /* Node in policy list */
};

struct res_user* res_user_new(const char *key);
void res_user_free(struct res_user *ru);

int res_user_setattr(struct res_user *ru, const char *name, const char *value);

int res_user_set_presence(struct res_user *ru, int presence);

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

int res_user_stat(struct res_user *ru, struct pwdb *pwdb, struct spdb *spdb);
int res_user_remediate(struct res_user *ru, struct pwdb *pwdb, struct spdb *spdb);

int res_user_is_pack(const char *packed);
char* res_user_pack(const struct res_user *ru);
struct res_user* res_user_unpack(const char *packed);

#endif
