#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "res_user.h"
#include "pack.h"
#include "mem.h"


#define RES_USER_PACK_PREFIX "res_user::"
#define RES_USER_PACK_OFFSET 10

/* Pack Format for res_user structure:
     a - ru_name
     a - ru_passwd
     L - ru_uid
     L - ru_gid
     a - ru_gecos
     a - ru_shell
     a - ru_dir
     C - ru_mkhome
     a - ru_skel
     C - ru_locked
     L - ru_pwmin
     L - ru_pwmax
     L - ru_pwwarn
     L - ru_inact
     L - ru_expire
 */
#define RES_USER_PACK_FORMAT "aaLLaaaCaCLLLLL"


static int _res_user_diff(struct res_user *ru);
static unsigned char _res_user_home_exists(struct res_user *ru);

/*****************************************************************/

static int _res_user_diff(struct res_user *ru)
{
	assert(ru);

	unsigned char locked = (ru->ru_sp->sp_pwdp && *(ru->ru_sp->sp_pwdp) == '!') ? 1 : 0;

	ru->ru_diff = RES_USER_NONE;

	if (res_user_enforced(ru, NAME) && strcmp(ru->ru_name, ru->ru_pw->pw_name) != 0) {
		ru->ru_diff |= RES_USER_NAME;
	}

	if (res_user_enforced(ru, PASSWD) && strcmp(ru->ru_passwd, ru->ru_sp->sp_pwdp) != 0) {
		ru->ru_diff |= RES_USER_PASSWD;
	}

	if (res_user_enforced(ru, UID) && ru->ru_uid != ru->ru_pw->pw_uid) {
		ru->ru_diff |= RES_USER_UID;
	}

	if (res_user_enforced(ru, GID) && ru->ru_gid != ru->ru_pw->pw_gid) {
		ru->ru_diff |= RES_USER_GID;
	}

	if (res_user_enforced(ru, GECOS) && strcmp(ru->ru_gecos, ru->ru_pw->pw_gecos) != 0) {
		ru->ru_diff |= RES_USER_GECOS;
	}

	if (res_user_enforced(ru, DIR) && strcmp(ru->ru_dir, ru->ru_pw->pw_dir) != 0) {
		ru->ru_diff |= RES_USER_DIR;
	}

	if (res_user_enforced(ru, SHELL) && strcmp(ru->ru_shell, ru->ru_pw->pw_shell) != 0) {
		ru->ru_diff |= RES_USER_SHELL;
	}

	if (ru->ru_mkhome == 1 && res_user_enforced(ru, MKHOME) && _res_user_home_exists(ru) == 0) {
		ru->ru_diff |= RES_USER_MKHOME;
	}

	if (res_user_enforced(ru, PWMIN) && ru->ru_pwmin != ru->ru_sp->sp_min) {
		ru->ru_diff |= RES_USER_PWMIN;
	}

	if (res_user_enforced(ru, PWMAX) && ru->ru_pwmax != ru->ru_sp->sp_max) {
		ru->ru_diff |= RES_USER_PWMAX;
	}

	if (res_user_enforced(ru, PWWARN) && ru->ru_pwwarn != ru->ru_sp->sp_warn) {
		ru->ru_diff |= RES_USER_PWWARN;
	}

	if (res_user_enforced(ru, INACT) && ru->ru_inact != ru->ru_sp->sp_inact) {
		ru->ru_diff |= RES_USER_INACT;
	}

	if (res_user_enforced(ru, EXPIRE) && ru->ru_expire != ru->ru_sp->sp_expire) {
		ru->ru_diff |= RES_USER_EXPIRE;
	}

	if (res_user_enforced(ru, LOCK) && ru->ru_lock != locked) {
		ru->ru_diff |= RES_USER_LOCK;
	}

	return 0;
}

static unsigned char _res_user_home_exists(struct res_user *ru)
{
	assert(ru);
	assert(ru->ru_dir);

	struct stat st;
	if (stat(ru->ru_dir, &st) == -1) {
		return 0; /* 0 = false */
	}

	return (S_ISDIR(st.st_mode) ? 1 : 0); /* 1 = true; 0 = false */
}

/*****************************************************************/

struct res_user* res_user_new(void)
{
	struct res_user *ru;

	ru = malloc(sizeof(struct res_user));
	if (!ru) {
		return NULL;
	}

	if (res_user_init(ru) != 0) {
		free(ru);
		return NULL;
	}

	return ru;
}

static int _res_user_init_params(struct res_user *ru)
{
	assert(ru);

	ru->ru_prio = 0;

	ru->ru_uid    = 0;
	ru->ru_uid    = 0;
	ru->ru_mkhome = 0;
	ru->ru_lock   = 1;
	ru->ru_pwmin  = 0;
	ru->ru_pwmax  = 0;
	ru->ru_pwwarn = 0;
	ru->ru_inact  = 0;
	ru->ru_expire = 0;

	ru->ru_enf = RES_USER_NONE;
	ru->ru_diff = RES_USER_NONE;

	return 0;
}

int  res_user_init(struct res_user *ru)
{
	assert(ru);

	_res_user_init_params(ru);
	list_init(&ru->res);

	ru->ru_name   = NULL;
	ru->ru_passwd = NULL;
	ru->ru_gecos  = NULL;
	ru->ru_dir    = NULL;
	ru->ru_shell  = NULL;
	ru->ru_skel   = NULL;

	ru->ru_pw     = NULL;
	ru->ru_sp     = NULL;

	return 0;
}

void res_user_deinit(struct res_user *ru)
{
	assert(ru);
	_res_user_init_params(ru);
	list_del(&ru->res);

	xfree(ru->ru_name);
	xfree(ru->ru_passwd);
	xfree(ru->ru_gecos);
	xfree(ru->ru_dir);
	xfree(ru->ru_shell);
	xfree(ru->ru_skel);

	ru->ru_pw = NULL;
	ru->ru_sp = NULL;
}

void res_user_free(struct res_user *ru)
{
	assert(ru);

	res_user_deinit(ru);
	free(ru);
}

int res_user_set_name(struct res_user *ru, const char *name)
{
	assert(ru);

	xfree(ru->ru_name);
	ru->ru_name = strdup(name);
	if (!ru->ru_name) { return -1; }

	ru->ru_enf |= RES_USER_NAME;
	return 0;
}

int res_user_unset_name(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_NAME;
	return 0;
}

int res_user_set_passwd(struct res_user *ru, const char *passwd)
{
	assert(ru);

	xfree(ru->ru_passwd);
	ru->ru_passwd = strdup(passwd);
	if (!ru->ru_passwd) { return -1; }

	ru->ru_enf |= RES_USER_PASSWD;
	return 0;
}

int res_user_unset_passwd(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_PASSWD;
	return 0;
}

int res_user_set_uid(struct res_user *ru, uid_t uid)
{
	assert(ru);

	ru->ru_uid = uid;

	ru->ru_enf |= RES_USER_UID;
	return 0;
}

int res_user_unset_uid(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_UID;
	return 0;
}

int res_user_set_gid(struct res_user *ru, gid_t gid)
{
	assert(ru);

	ru->ru_gid = gid;

	ru->ru_enf |= RES_USER_GID;
	return 0;
}

int res_user_unset_gid(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_GID;
	return 0;
}

int res_user_set_gecos(struct res_user *ru, const char *gecos)
{
	assert(ru);

	xfree(ru->ru_gecos);
	ru->ru_gecos = strdup(gecos);
	if (!ru->ru_gecos) { return -1; }

	ru->ru_enf |= RES_USER_GECOS;
	return 0;
}

int res_user_unset_gecos(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_GECOS;
	return 0;
}

int res_user_set_dir(struct res_user *ru, const char *path)
{
	assert(ru);

	xfree(ru->ru_dir);
	ru->ru_dir = strdup(path);
	if (!ru->ru_dir) { return -1; }

	ru->ru_enf |= RES_USER_DIR;
	return 0;
}

int res_user_unset_dir(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_DIR;
	return 0;
}

int res_user_set_shell(struct res_user *ru, const char *shell)
{
	assert(ru);

	xfree(ru->ru_shell);
	ru->ru_shell = strdup(shell);
	if (!ru->ru_shell) { return -1; }

	ru->ru_enf |= RES_USER_SHELL;
	return 0;
}

int res_user_unset_shell(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_SHELL;
	return 0;
}

int res_user_set_makehome(struct res_user *ru, unsigned char mkhome, const char *skel)
{
	assert(ru);

	ru->ru_mkhome = mkhome;
	xfree(ru->ru_skel); /* nullifies ru_skel */
	if (mkhome && skel) {
		ru->ru_skel = strdup(skel);
	}

	ru->ru_enf |= RES_USER_MKHOME;
	return 0;
}

int res_user_unset_makehome(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_MKHOME;
	return 0;
}

int res_user_set_pwmin(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_pwmin = days;

	ru->ru_enf |= RES_USER_PWMIN;
	return 0;
}

int res_user_unset_pwmin(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_PWMIN;
	return 0;
}

int res_user_set_pwmax(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_pwmax = days;

	ru->ru_enf |= RES_USER_PWMAX;
	return 0;
}

int res_user_unset_pwmax(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_PWMAX;
	return 0;
}

int res_user_set_pwwarn(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_pwwarn = days;

	ru->ru_enf |= RES_USER_PWWARN;
	return 0;
}

int res_user_unset_pwwarn(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_PWWARN;
	return 0;
}

int res_user_set_inact(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_inact = days;

	ru->ru_enf |= RES_USER_INACT;
	return 0;
}

int res_user_unset_inact(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_INACT;
	return 0;
}

int res_user_set_expire(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_expire = days;

	ru->ru_enf |= RES_USER_EXPIRE;
	return 0;
}

int res_user_unset_expire(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_EXPIRE;
	return 0;
}

int res_user_set_lock(struct res_user *ru, unsigned char locked)
{
	assert(ru);

	ru->ru_lock = locked;

	ru->ru_enf |= RES_USER_LOCK;
	return 0;
}

int res_user_unset_lock(struct res_user *ru)
{
	assert(ru);

	ru->ru_enf ^= RES_USER_LOCK;
	return 0;
}

void res_user_merge(struct res_user *ru1, struct res_user *ru2)
{
	assert(ru1);
	assert(ru2);

	struct res_user *tmp;

	if (ru1->ru_prio > ru2->ru_prio) {
		/* out-of-order, swap pointers */
		tmp = ru1; ru1 = ru2; ru2 = ru1; tmp = NULL;
	}

	/* ru1 as priority over ru2 */
	assert(ru1->ru_prio <= ru2->ru_prio);

	if ( res_user_enforced(ru2, NAME) &&
	    !res_user_enforced(ru1, NAME)) {
		res_user_set_name(ru1, ru2->ru_name);
	}

	if ( res_user_enforced(ru2, PASSWD) &&
	    !res_user_enforced(ru1, PASSWD)) {
		res_user_set_passwd(ru1, ru2->ru_passwd);
	}

	if ( res_user_enforced(ru2, UID) &&
	    !res_user_enforced(ru1, UID)) {
		res_user_set_uid(ru1, ru2->ru_uid);
	}

	if ( res_user_enforced(ru2, GID) &&
	    !res_user_enforced(ru1, GID)) {
		res_user_set_gid(ru1, ru2->ru_gid);
	}

	if ( res_user_enforced(ru2, GECOS) &&
	    !res_user_enforced(ru1, GECOS)) {
		res_user_set_gecos(ru1, ru2->ru_gecos);
	}

	if ( res_user_enforced(ru2, DIR) &&
	    !res_user_enforced(ru1, DIR)) {
		res_user_set_dir(ru1, ru2->ru_dir);
	}

	if ( res_user_enforced(ru2, SHELL) &&
	    !res_user_enforced(ru1, SHELL)) {
		res_user_set_shell(ru1, ru2->ru_shell);
	}

	if ( res_user_enforced(ru2, MKHOME) &&
	    !res_user_enforced(ru1, MKHOME)) {
		res_user_set_makehome(ru1, ru2->ru_mkhome, ru2->ru_skel);
	}

	if ( res_user_enforced(ru2, PWMIN) &&
	    !res_user_enforced(ru1, PWMIN)) {
		res_user_set_pwmin(ru1, ru2->ru_pwmin);
	}

	if ( res_user_enforced(ru2, PWMAX) &&
	    !res_user_enforced(ru1, PWMAX)) {
		res_user_set_pwmax(ru1, ru2->ru_pwmax);
	}

	if ( res_user_enforced(ru2, PWWARN) &&
	    !res_user_enforced(ru1, PWWARN)) {
		res_user_set_pwwarn(ru1, ru2->ru_pwwarn);
	}

	if ( res_user_enforced(ru2, INACT) &&
	    !res_user_enforced(ru1, INACT)) {
		res_user_set_inact(ru1, ru2->ru_inact);
	}

	if ( res_user_enforced(ru2, EXPIRE) &&
	    !res_user_enforced(ru1, EXPIRE)) {
		res_user_set_expire(ru1, ru2->ru_expire);
	}

	if ( res_user_enforced(ru2, LOCK) &&
	    !res_user_enforced(ru1, LOCK)) {
		res_user_set_lock(ru1, ru2->ru_lock);
	}
}

int res_user_stat(struct res_user *ru, struct pwdb *pwdb, struct spdb *spdb)
{
	assert(ru);
	assert(pwdb);
	assert(spdb);

	ru->ru_pw = pwdb_get_by_name(pwdb, ru->ru_name);
	ru->ru_sp = spdb_get_by_name(spdb, ru->ru_name);
	if (!ru->ru_pw || !ru->ru_sp) { /* new account */
		ru->ru_diff = ru->ru_enf;
		return 0;
	}

	return _res_user_diff(ru);
}

int res_user_remediate(struct res_user *ru, struct pwdb *pwdb, struct spdb *spdb)
{
	assert(ru);
	assert(pwdb);
	assert(spdb);

	if (!ru->ru_pw) {
		ru->ru_pw = pwdb_new_entry(pwdb, ru->ru_name);
		if (!ru->ru_pw) { return -1; }
	}

	if (!ru->ru_sp) {
		ru->ru_sp = spdb_new_entry(spdb, ru->ru_name);
		if (!ru->ru_sp) { return -1; }
	}

	if (res_user_enforced(ru, PASSWD)) {
		xfree(ru->ru_pw->pw_passwd);
		ru->ru_pw->pw_passwd = strdup("x");

		xfree(ru->ru_sp->sp_pwdp);
		ru->ru_sp->sp_pwdp = strdup(ru->ru_passwd);
	}

	if (res_user_enforced(ru, UID)) {
		ru->ru_pw->pw_uid = ru->ru_uid;
	}

	if (res_user_enforced(ru, GID)) {
		ru->ru_pw->pw_gid = ru->ru_gid;
	}

	if (res_user_enforced(ru, GECOS)) {
		xfree(ru->ru_pw->pw_gecos);
		ru->ru_pw->pw_gecos = strdup(ru->ru_gecos);
	}

	if (res_user_enforced(ru, DIR)) {
		xfree(ru->ru_pw->pw_dir);
		ru->ru_pw->pw_dir = strdup(ru->ru_dir);
	}

	if (res_user_enforced(ru, SHELL)) {
		xfree(ru->ru_pw->pw_shell);
		ru->ru_pw->pw_shell = strdup(ru->ru_shell);
	}

	if (res_user_enforced(ru, PWMIN)) {
		ru->ru_sp->sp_min = ru->ru_pwmin;
	}

	if (res_user_enforced(ru, PWMAX)) {
		ru->ru_sp->sp_max = ru->ru_pwmax;
	}

	if (res_user_enforced(ru, PWWARN)) {
		ru->ru_sp->sp_warn = ru->ru_pwwarn;
	}

	if (res_user_enforced(ru, INACT)) {
		ru->ru_sp->sp_inact = ru->ru_inact;
	}

	if (res_user_enforced(ru, EXPIRE)) {
		ru->ru_sp->sp_expire = ru->ru_expire;
	}

	return 0;
}

int res_user_is_pack(const char *packed)
{
	return strncmp(packed, RES_USER_PACK_PREFIX, RES_USER_PACK_OFFSET);
}
char* res_user_pack(const struct res_user *ru)
{
	char *packed;
	size_t pack_len;

	pack_len = pack(NULL, 0, RES_USER_PACK_FORMAT,
		ru->ru_name,   ru->ru_passwd, ru->ru_uid,    ru->ru_gid,
		ru->ru_gecos,  ru->ru_shell,  ru->ru_dir,    ru->ru_mkhome,
		ru->ru_skel,   ru->ru_lock,   ru->ru_pwmin,  ru->ru_pwmax,
		ru->ru_pwwarn, ru->ru_inact,  ru->ru_expire);

	packed = malloc(pack_len + RES_USER_PACK_OFFSET);
	strncpy(packed, RES_USER_PACK_PREFIX, pack_len);

	pack(packed + RES_USER_PACK_OFFSET, pack_len, RES_USER_PACK_FORMAT,
		ru->ru_name,   ru->ru_passwd, ru->ru_uid,    ru->ru_gid,
		ru->ru_gecos,  ru->ru_shell,  ru->ru_dir,    ru->ru_mkhome,
		ru->ru_skel,   ru->ru_lock,   ru->ru_pwmin,  ru->ru_pwmax,
		ru->ru_pwwarn, ru->ru_inact,  ru->ru_expire);

	return packed;
}

struct res_user* res_user_unpack(const char *packed)
{
	struct res_user *ru;

	if (strncmp(packed, RES_USER_PACK_PREFIX, RES_USER_PACK_OFFSET) != 0) {
		return NULL;
	}

	ru = res_user_new();
	if (unpack(packed + RES_USER_PACK_OFFSET, RES_USER_PACK_FORMAT,
		&ru->ru_name,   &ru->ru_passwd, &ru->ru_uid,    &ru->ru_gid,
		&ru->ru_gecos,  &ru->ru_shell,  &ru->ru_dir,    &ru->ru_mkhome,
		&ru->ru_skel,   &ru->ru_lock,   &ru->ru_pwmin,  &ru->ru_pwmax,
		&ru->ru_pwwarn, &ru->ru_inact,  &ru->ru_expire) != 0) {

		res_user_free(ru);
		return NULL;
	}

	return ru;
}
