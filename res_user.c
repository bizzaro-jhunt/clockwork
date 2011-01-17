#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fts.h>
#include <fcntl.h>
#include <unistd.h>

#include "resource.h"
#include "res_user.h"
#include "pack.h"
#include "mem.h"


#define RES_USER_PACK_PREFIX "res_user::"
#define RES_USER_PACK_OFFSET 10

/* Pack Format for res_user structure:
     L - ru_enf
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
#define RES_USER_PACK_FORMAT "LaaLLaaaCaCLLLLL"


static int _res_user_diff(struct res_user *ru);
static unsigned char _res_user_home_exists(struct res_user *ru);
static int _res_user_populate_home(const char *home, const char *skel, uid_t uid, gid_t gid);

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

static int _res_user_populate_home(const char *home, const char *skel, uid_t uid, gid_t gid)
{
	FTS *fts;
	FTSENT *ent;
	char *path_argv[2] = { strdup(skel), NULL };
	char *skel_path, *home_path;
	int skel_fd, home_fd;
	mode_t mode;
	char buf[8192];
	size_t nread;

	fts = fts_open(path_argv, FTS_PHYSICAL, NULL);
	if (!fts) {
		free(path_argv[0]);
		return -1;
	}

	while ( (ent = fts_read(fts)) != NULL ) {
		if (strcmp(ent->fts_accpath, ent->fts_path) != 0) {
			home_path = string("%s/%s", home, ent->fts_accpath);
			skel_path = string("%s/%s", skel, ent->fts_accpath);
			mode = ent->fts_statp->st_mode & 0777;

			if (S_ISREG(ent->fts_statp->st_mode)) {
				skel_fd = open(skel_path, O_RDONLY); /* FIXME: check retval */
				home_fd = creat(home_path, mode); /* FIXME: check retval */
				chown(home_path, uid, gid); /* FIXME: check retval */

				while ((nread = read(skel_fd, buf, 8192)) > 0)
					write(home_fd, buf, nread);

			} else if (S_ISDIR(ent->fts_statp->st_mode)) {
				mkdir(home_path, mode); /* FIXME: check retval */
				chown(home_path, uid, gid); /* FIXME: check retval */
			}

			free(home_path);
			free(skel_path);
		}
	}

	free(path_argv[0]);
	return 0;
}

/*****************************************************************/

struct res_user* res_user_new(const char *key)
{
	struct res_user *ru;

	ru = malloc(sizeof(struct res_user));
	if (!ru) {
		return NULL;
	}

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

	list_init(&ru->res);

	ru->ru_name   = NULL;
	ru->ru_passwd = NULL;
	ru->ru_gecos  = NULL;
	ru->ru_dir    = NULL;
	ru->ru_shell  = NULL;
	ru->ru_skel   = NULL;

	ru->ru_pw     = NULL;
	ru->ru_sp     = NULL;

	if (key) {
		res_user_set_name(ru, key);
		ru->key = resource_key("res_user", key);
	} else {
		ru->key = NULL;
	}

	return ru;
}

void res_user_free(struct res_user *ru)
{
	if (ru) {
		list_del(&ru->res);

		free(ru->ru_name);
		free(ru->ru_passwd);
		free(ru->ru_gecos);
		free(ru->ru_dir);
		free(ru->ru_shell);
		free(ru->ru_skel);

		free(ru->key);
	}
	free(ru);
}

int res_user_setattr(struct res_user *ru, const char *name, const char *value)
{
	if (strcmp(name, "uid") == 0) {
		return res_user_set_uid(ru, strtoll(value, NULL, 10));
	} else if (strcmp(name, "gid") == 0) {
		return res_user_set_gid(ru, strtoll(value, NULL, 10));
	} else if (strcmp(name, "home") == 0) {
		return res_user_set_dir(ru, value);
	} else if (strcmp(name, "present") == 0) {
		return res_user_set_presence(ru, strcmp(value, "no"));
	} else if (strcmp(name, "locked") == 0) {
		return res_user_set_lock(ru, strcmp(value, "no"));
	} else if (strcmp(name, "gecos") == 0 || strcmp(name, "comment") == 0) {
		return res_user_set_gecos(ru, value);
	} else if (strcmp(name, "shell") == 0) {
		return res_user_set_shell(ru, value);
	} else if (strcmp(name, "pwhash") == 0 || strcmp(name, "password") == 0) {
		return res_user_set_passwd(ru, value);
	} else if (strcmp(name, "pwmin") == 0) { // FIXME: need better key
		return res_user_set_pwmin(ru, strtoll(value, NULL, 10));
	} else if (strcmp(name, "pwmax") == 0) { // FIXME: need better key
		return res_user_set_pwmax(ru, strtoll(value, NULL, 10));
	} else if (strcmp(name, "pwwarn") == 0) { // FIXME: need better key
		return res_user_set_pwwarn(ru, strtoll(value, NULL, 10));
	} else if (strcmp(name, "pwinact") == 0) { // FIXME: need better key
		return res_user_set_inact(ru, strtoll(value, NULL, 10));
	} else if (strcmp(name, "expiry") == 0 || strcmp(name, "expiration") == 0) {
		return res_user_set_expire(ru, strtoll(value, NULL, 10));
	} else if (strcmp(name, "locked") == 0) {
		return res_user_set_lock(ru, strcmp(value, "no"));
	} else if (strcmp(name, "skeleton") == 0 || strcmp(name, "makehome") == 0) {
		if (strcmp(value, "no") == 0) {
			return res_user_set_makehome(ru, 0, NULL);
		} else if (strcmp(value, "yes") == 0) {
			return res_user_set_makehome(ru, 1, "/etc/skel");
		} else {
			return res_user_set_makehome(ru, 1, value);
		}
	}

	return -1;
}

int res_user_set_presence(struct res_user *ru, int presence)
{
	assert(ru);

	if (presence) {
		ru->ru_enf ^= RES_USER_ABSENT;
	} else {
		ru->ru_enf |= RES_USER_ABSENT;
	}

	return 0;
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

int res_user_set_passwd(struct res_user *ru, const char *passwd)
{
	assert(ru);

	xfree(ru->ru_passwd);
	ru->ru_passwd = strdup(passwd);
	if (!ru->ru_passwd) { return -1; }

	ru->ru_enf |= RES_USER_PASSWD;
	return 0;
}

int res_user_set_uid(struct res_user *ru, uid_t uid)
{
	assert(ru);

	ru->ru_uid = uid;

	ru->ru_enf |= RES_USER_UID;
	return 0;
}

int res_user_set_gid(struct res_user *ru, gid_t gid)
{
	assert(ru);

	ru->ru_gid = gid;

	ru->ru_enf |= RES_USER_GID;
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

int res_user_set_dir(struct res_user *ru, const char *path)
{
	assert(ru);

	xfree(ru->ru_dir);
	ru->ru_dir = strdup(path);
	if (!ru->ru_dir) { return -1; }

	ru->ru_enf |= RES_USER_DIR;
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

int res_user_set_pwmin(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_pwmin = days;

	ru->ru_enf |= RES_USER_PWMIN;
	return 0;
}

int res_user_set_pwmax(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_pwmax = days;

	ru->ru_enf |= RES_USER_PWMAX;
	return 0;
}

int res_user_set_pwwarn(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_pwwarn = days;

	ru->ru_enf |= RES_USER_PWWARN;
	return 0;
}

int res_user_set_inact(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_inact = days;

	ru->ru_enf |= RES_USER_INACT;
	return 0;
}

int res_user_set_expire(struct res_user *ru, long days)
{
	assert(ru);

	ru->ru_expire = days;

	ru->ru_enf |= RES_USER_EXPIRE;
	return 0;
}

int res_user_set_lock(struct res_user *ru, unsigned char locked)
{
	assert(ru);

	ru->ru_lock = locked;

	ru->ru_enf |= RES_USER_LOCK;
	return 0;
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

struct report* res_user_remediate(struct res_user *ru, int dryrun, struct pwdb *pwdb, struct spdb *spdb)
{
	assert(ru);
	assert(pwdb);
	assert(spdb);

	struct report *report;
	char *action;

	int new_user = 0;

	report = report_new("User", ru->ru_name);

	/* Remove the user if RES_USER_ABSENT */
	if (res_user_enforced(ru, ABSENT)) {
		if (ru->ru_pw || ru->ru_sp) {
			action = string("remove user");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
				return report;
			}

			if ((ru->ru_pw && pwdb_rm(pwdb, ru->ru_pw) != 0)
			 || (ru->ru_sp && spdb_rm(spdb, ru->ru_sp) != 0)) {
				report_action(report, action, ACTION_FAILED);
			} else {
				report_action(report, action, ACTION_SUCCEEDED);
			}
		}

		return report;
	}

	if (!ru->ru_pw || !ru->ru_sp) {
		action = string("create user");
		new_user = 1;

		if (!ru->ru_pw) { ru->ru_pw = pwdb_new_entry(pwdb, ru->ru_name); }
		if (!ru->ru_sp) { ru->ru_sp = spdb_new_entry(spdb, ru->ru_name); }

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (ru->ru_pw && ru->ru_sp) {
				report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
			return report;
		}
	}

	if (res_user_different(ru, PASSWD)) {
		if (new_user) {
			action = string("set user password");
		} else {
			action = string("change user password");
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			xfree(ru->ru_pw->pw_passwd);
			ru->ru_pw->pw_passwd = strdup("x");

			xfree(ru->ru_sp->sp_pwdp);
			ru->ru_sp->sp_pwdp = strdup(ru->ru_passwd);

			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, UID)) {
		if (new_user) {
			action = string("set uid to %u", ru->ru_uid);
		} else {
			action = string("change uid from %u to %u", ru->ru_pw->pw_uid, ru->ru_uid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			ru->ru_pw->pw_uid = ru->ru_uid;
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, GID)) {
		if (new_user) {
			action = string("set gid to %u", ru->ru_gid);
		} else {
			action = string("change gid from %u to %u", ru->ru_pw->pw_gid, ru->ru_gid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			ru->ru_pw->pw_gid = ru->ru_gid;
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, GECOS)) {
		if (new_user) {
			action = string("set GECOS to %s", ru->ru_gecos);
		} else {
			action = string("change GECOS to %s", ru->ru_gecos);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			xfree(ru->ru_pw->pw_gecos);
			ru->ru_pw->pw_gecos = strdup(ru->ru_gecos);

			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, DIR)) {
		if (new_user) {
			action = string("set home directory to %s", ru->ru_dir);
		} else {
			action = string("change home from %s to %s", ru->ru_pw->pw_dir, ru->ru_dir);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			xfree(ru->ru_pw->pw_dir);
			ru->ru_pw->pw_dir = strdup(ru->ru_dir);
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, MKHOME)) {
		action = string("create home directory");

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			/* mkdir $home; populate from $skel */
			mkdir(ru->ru_dir, 0700); /* FIXME: check return value */
			chown(ru->ru_dir, ru->ru_pw->pw_uid, ru->ru_pw->pw_gid); /* FIXME: check return value */

			report_action(report, action, ACTION_SUCCEEDED);
		}

		if (ru->ru_skel) {
			action = string("populate home directory from %s", ru->ru_skel);

			if (dryrun) {
				report_action(report, string("populate home directory from %s", ru->ru_skel), ACTION_SKIPPED);
			} else {
				/* copy *all* files from ru_skel into ru_dir */
				_res_user_populate_home(ru->ru_dir, ru->ru_skel, ru->ru_pw->pw_uid, ru->ru_pw->pw_gid);
				report_action(report, action, ACTION_SUCCEEDED);
			}
		}
	}

	if (res_user_different(ru, SHELL)) {
		if (new_user) {
			action = string("set login shell to %s", ru->ru_shell);
		} else {
			action = string("change shell from %s to %s", ru->ru_pw->pw_shell, ru->ru_shell);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			xfree(ru->ru_pw->pw_shell);
			ru->ru_pw->pw_shell = strdup(ru->ru_shell);
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, PWMIN)) {
		if (new_user) {
			action = string("set password minimum age to %u days", ru->ru_pwmin);
		} else {
			action = string("change password minimum age to %u days", ru->ru_pwmin);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			ru->ru_sp->sp_min = ru->ru_pwmin;
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, PWMAX)) {
		if (new_user) {
			action = string("set password maximum age to %u days", ru->ru_pwmax);
		} else {
			action = string("change password maximum age to %u days", ru->ru_pwmax);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			ru->ru_sp->sp_max = ru->ru_pwmax;
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, PWWARN)) {
		if (new_user) {
			action = string("set password expiry warning to %u days", ru->ru_pwwarn);
		} else {
			action = string("change password expiry warning to %u days", ru->ru_pwwarn);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			ru->ru_sp->sp_warn = ru->ru_pwwarn;
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, INACT)) {
		if (ru->ru_inact) {
			action = string("deactivate account");
		} else {
			action = string("activate account");
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			ru->ru_sp->sp_inact = ru->ru_inact;
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, EXPIRE)) {
		if (new_user) {
			action = string("set account expiration to %u", ru->ru_expire);
		} else {
			action = string("change account expiration to %u", ru->ru_expire);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			ru->ru_sp->sp_expire = ru->ru_expire;
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (res_user_different(ru, LOCK)) {
		size_t len = strlen(ru->ru_sp->sp_pwdp) + 2;
		char pbuf[len], *password;
		memcpy(pbuf + 1, ru->ru_sp->sp_pwdp, len - 1);

		if (ru->ru_lock) {
			action = string("lock account");
			pbuf[0] = '!';
			password = pbuf;
		} else {
			action = string("unlock account");
			password = pbuf + 1;
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			xfree(ru->ru_sp->sp_pwdp);
			ru->ru_sp->sp_pwdp = strdup(password);

			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	return report;
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
		ru->ru_enf,
		ru->ru_name,   ru->ru_passwd, ru->ru_uid,    ru->ru_gid,
		ru->ru_gecos,  ru->ru_shell,  ru->ru_dir,    ru->ru_mkhome,
		ru->ru_skel,   ru->ru_lock,   ru->ru_pwmin,  ru->ru_pwmax,
		ru->ru_pwwarn, ru->ru_inact,  ru->ru_expire);

	packed = malloc(pack_len + RES_USER_PACK_OFFSET);
	strncpy(packed, RES_USER_PACK_PREFIX, RES_USER_PACK_OFFSET);

	pack(packed + RES_USER_PACK_OFFSET, pack_len, RES_USER_PACK_FORMAT,
		ru->ru_enf,
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

	ru = res_user_new(NULL);
	if (unpack(packed + RES_USER_PACK_OFFSET, RES_USER_PACK_FORMAT,
		&ru->ru_enf,
		&ru->ru_name,   &ru->ru_passwd, &ru->ru_uid,    &ru->ru_gid,
		&ru->ru_gecos,  &ru->ru_shell,  &ru->ru_dir,    &ru->ru_mkhome,
		&ru->ru_skel,   &ru->ru_lock,   &ru->ru_pwmin,  &ru->ru_pwmax,
		&ru->ru_pwwarn, &ru->ru_inact,  &ru->ru_expire) != 0) {

		res_user_free(ru);
		return NULL;
	}

	ru->key = resource_key("res_user", ru->ru_name);

	return ru;
}
