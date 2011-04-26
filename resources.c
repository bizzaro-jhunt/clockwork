#include "resources.h"
#include "pack.h"

#include <fts.h>
#include <fcntl.h>

#define _FD2FD_CHUNKSIZE 16384

static int _res_user_populate_home(const char *home, const char *skel, uid_t uid, gid_t gid);
static int _res_file_fd2fd(int dest, int src, ssize_t bytes);
static int _group_update(stringlist*, stringlist*, const char*);

/*****************************************************************/

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
				skel_fd = open(skel_path, O_RDONLY);
				home_fd = creat(home_path, mode);

				if (skel_fd >= 0 && home_fd >= 0) {
					chown(home_path, uid, gid);
					while ((nread = read(skel_fd, buf, 8192)) > 0)
						write(home_fd, buf, nread);
				}

			} else if (S_ISDIR(ent->fts_statp->st_mode)) {
				if (mkdir(home_path, mode) == 0) {
					chown(home_path, uid, gid);
					chmod(home_path, 0755);
				}
			}

			free(home_path);
			free(skel_path);
		}
	}

	free(path_argv[0]);
	return 0;
}

static int _res_file_fd2fd(int dest, int src, ssize_t bytes)
{
	char buf[_FD2FD_CHUNKSIZE];
	ssize_t nread = 0;

	while (bytes > 0) {
		nread = (_FD2FD_CHUNKSIZE > bytes ? bytes : _FD2FD_CHUNKSIZE);

		if ((nread = read(src, buf, nread)) <= 0
		 || write(dest, buf, nread) != nread) {
			return -1;
		}

		bytes -= nread;
	}

	return 0;
}

static int _group_update(stringlist *add, stringlist *rm, const char *user)
{
	/* put user in add */
	if (stringlist_search(add, user) != 0) {
		stringlist_add(add, user);
	}

	/* take user out of rm */
	if (stringlist_search(rm, user) == 0) {
		stringlist_remove(rm, user);
	}

	return 0;
}

/*****************************************************************/

void* res_user_new(const char *key)
{
	struct res_user *ru;

	ru = xmalloc(sizeof(struct res_user));

	ru->ru_uid    = -1;
	ru->ru_uid    = -1;
	ru->ru_mkhome = 0;
	ru->ru_lock   = 1;
	ru->ru_pwmin  = 0;
	ru->ru_pwmax  = 0;
	ru->ru_pwwarn = 0;
	ru->ru_inact  = 0;
	ru->ru_expire = 0;

	ru->enforced = RES_USER_NONE;
	ru->different = RES_USER_NONE;

	list_init(&ru->res);

	ru->ru_name   = NULL;
	ru->ru_passwd = NULL;
	ru->ru_gecos  = NULL;
	ru->ru_dir    = NULL;
	ru->ru_shell  = NULL;
	ru->ru_skel   = NULL;

	ru->ru_pw     = NULL;
	ru->ru_sp     = NULL;

	ru->key = NULL;
	if (key) {
		ru->key = strdup(key);
		res_user_set(ru, "username", key);
	}

	return ru;
}

void res_user_free(void *res)
{
	struct res_user *ru = (struct res_user*)(res);

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

char* res_user_key(const void *res)
{
	const struct res_user *ru = (struct res_user*)(res);
	assert(ru);

	return string("res_user:%s", ru->key);
}

int res_user_norm(void *res, struct policy *pol) { return 0; }

int res_user_set(void *res, const char *name, const char *value)
{
	struct res_user *ru = (struct res_user*)(res);
	assert(ru);

	if (strcmp(name, "uid") == 0) {
		ru->ru_uid = strtoll(value, NULL, 10);
		ru->enforced |= RES_USER_UID;

	} else if (strcmp(name, "gid") == 0) {
		ru->ru_gid = strtoll(value, NULL, 10);
		ru->enforced |= RES_USER_GID;

	} else if (strcmp(name, "username") == 0) {
		free(ru->ru_name);
		ru->ru_name = strdup(value);
		ru->enforced |= RES_USER_NAME;

	} else if (strcmp(name, "home") == 0) {
		free(ru->ru_dir);
		ru->ru_dir = strdup(value);
		ru->enforced |= RES_USER_DIR;

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") != 0) {
			ru->enforced ^= RES_USER_ABSENT;
		} else {
			ru->enforced |= RES_USER_ABSENT;
		}

	} else if (strcmp(name, "locked") == 0) {
		ru->ru_lock = strcmp(value, "no") ? 1 : 0;
		ru->enforced |= RES_USER_LOCK;

	} else if (strcmp(name, "gecos") == 0 || strcmp(name, "comment") == 0) {
		free(ru->ru_gecos);
		ru->ru_gecos = strdup(value);
		ru->enforced |= RES_USER_GECOS;

	} else if (strcmp(name, "shell") == 0) {
		free(ru->ru_shell);
		ru->ru_shell = strdup(value);
		ru->enforced |= RES_USER_SHELL;

	} else if (strcmp(name, "pwhash") == 0 || strcmp(name, "password") == 0) {
		free(ru->ru_passwd);
		ru->ru_passwd = strdup(value);
		ru->enforced |= RES_USER_PASSWD;

	} else if (strcmp(name, "pwmin") == 0) { // FIXME: need better key
		ru->ru_pwmin = strtoll(value, NULL, 10);
		ru->enforced |= RES_USER_PWMIN;

	} else if (strcmp(name, "pwmax") == 0) { // FIXME: need better key
		ru->ru_pwmax = strtoll(value, NULL, 10);
		ru->enforced |= RES_USER_PWMAX;

	} else if (strcmp(name, "pwwarn") == 0) { // FIXME: need better key
		ru->ru_pwwarn = strtoll(value, NULL, 10);
		ru->enforced |= RES_USER_PWWARN;

	} else if (strcmp(name, "inact") == 0) { // FIXME: need better key
		ru->ru_inact = strtoll(value, NULL, 10);
		ru->enforced |= RES_USER_INACT;

	} else if (strcmp(name, "expiry") == 0 || strcmp(name, "expiration") == 0) {
		ru->ru_expire = strtoll(value, NULL, 10);
		ru->enforced |= RES_USER_EXPIRE;

	} else if (strcmp(name, "skeleton") == 0 || strcmp(name, "makehome") == 0) {
		ru->enforced |= RES_USER_MKHOME;
		xfree(ru->ru_skel);
		ru->ru_mkhome = (strcmp(value, "no") ? 1 : 0);

		if (!ru->ru_mkhome) { return 0; }
		ru->ru_skel = strdup(strcmp(value, "yes") == 0 ? "/etc/skel" : value);

	}

	return 0;
}

int res_user_match(const void *res, const char *name, const char *value)
{
	const struct res_user *ru = (const struct res_user*)(res);
	assert(ru);

	char *test_value;
	int rc;

	if (strcmp(name, "uid") == 0) {
		test_value = string("%u", ru->ru_uid);
	} else if (strcmp(name, "gid") == 0) {
		test_value = string("%u", ru->ru_gid);
	} else if (strcmp(name, "username") == 0) {
		test_value = string("%s", ru->ru_name);
	} else if (strcmp(name, "home") == 0) {
		test_value = string("%s", ru->ru_dir);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_user_stat(void *res, const struct resource_env *env)
{
	struct res_user *ru = (struct res_user*)(res);
	unsigned char locked;
	struct stat home;

	assert(ru);
	assert(env);
	assert(env->user_pwdb);
	assert(env->user_spdb);

	ru->ru_pw = pwdb_get_by_name(env->user_pwdb, ru->ru_name);
	ru->ru_sp = spdb_get_by_name(env->user_spdb, ru->ru_name);
	if (!ru->ru_pw || !ru->ru_sp) { /* new account */
		ru->different = ru->enforced;
		return 0;
	}

	locked = (ru->ru_sp->sp_pwdp && *(ru->ru_sp->sp_pwdp) == '!') ? 1 : 0;
	ru->different = RES_USER_NONE;

	if (ENFORCED(ru, RES_USER_NAME) && strcmp(ru->ru_name, ru->ru_pw->pw_name) != 0) {
		ru->different |= RES_USER_NAME;
	}

	if (ENFORCED(ru, RES_USER_PASSWD) && strcmp(ru->ru_passwd, ru->ru_sp->sp_pwdp) != 0) {
		ru->different |= RES_USER_PASSWD;
	}

	if (ENFORCED(ru, RES_USER_UID) && ru->ru_uid != ru->ru_pw->pw_uid) {
		ru->different |= RES_USER_UID;
	}

	if (ENFORCED(ru, RES_USER_GID) && ru->ru_gid != ru->ru_pw->pw_gid) {
		ru->different |= RES_USER_GID;
	}

	if (ENFORCED(ru, RES_USER_GECOS) && strcmp(ru->ru_gecos, ru->ru_pw->pw_gecos) != 0) {
		ru->different |= RES_USER_GECOS;
	}

	if (ENFORCED(ru, RES_USER_DIR) && strcmp(ru->ru_dir, ru->ru_pw->pw_dir) != 0) {
		ru->different |= RES_USER_DIR;
	}

	if (ENFORCED(ru, RES_USER_SHELL) && strcmp(ru->ru_shell, ru->ru_pw->pw_shell) != 0) {
		ru->different |= RES_USER_SHELL;
	}

	if (ru->ru_mkhome == 1 && ENFORCED(ru, RES_USER_MKHOME)
	 && (stat(ru->ru_dir, &home) != 0 || S_ISDIR(home.st_mode))) {
		ru->different |= RES_USER_MKHOME;
	}

	if (ENFORCED(ru, RES_USER_PWMIN) && ru->ru_pwmin != ru->ru_sp->sp_min) {
		ru->different |= RES_USER_PWMIN;
	}

	if (ENFORCED(ru, RES_USER_PWMAX) && ru->ru_pwmax != ru->ru_sp->sp_max) {
		ru->different |= RES_USER_PWMAX;
	}

	if (ENFORCED(ru, RES_USER_PWWARN) && ru->ru_pwwarn != ru->ru_sp->sp_warn) {
		ru->different |= RES_USER_PWWARN;
	}

	if (ENFORCED(ru, RES_USER_INACT) && ru->ru_inact != ru->ru_sp->sp_inact) {
		ru->different |= RES_USER_INACT;
	}

	if (ENFORCED(ru, RES_USER_EXPIRE) && ru->ru_expire != ru->ru_sp->sp_expire) {
		ru->different |= RES_USER_EXPIRE;
	}

	if (ENFORCED(ru, RES_USER_LOCK) && ru->ru_lock != locked) {
		ru->different |= RES_USER_LOCK;
	}

	return 0;
}

struct report* res_user_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_user *ru = (struct res_user*)(res);
	assert(ru);
	assert(env);
	assert(env->user_pwdb);
	assert(env->user_spdb);

	struct report *report;
	char *action;

	int new_user = 0;

	report = report_new("User", ru->ru_name);

	/* Remove the user if RES_USER_ABSENT */
	if (ENFORCED(ru, RES_USER_ABSENT)) {
		if (ru->ru_pw || ru->ru_sp) {
			action = string("remove user");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
				return report;
			}

			if ((ru->ru_pw && pwdb_rm(env->user_pwdb, ru->ru_pw) != 0)
			 || (ru->ru_sp && spdb_rm(env->user_spdb, ru->ru_sp) != 0)) {
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

		if (!ru->ru_pw) { ru->ru_pw = pwdb_new_entry(env->user_pwdb, ru->ru_name, ru->ru_uid, ru->ru_gid); }
		if (!ru->ru_sp) { ru->ru_sp = spdb_new_entry(env->user_spdb, ru->ru_name); }

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (ru->ru_pw && ru->ru_sp) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
			return report;
		}
	}

	if (DIFFERENT(ru, RES_USER_PASSWD)) {
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

	if (DIFFERENT(ru, RES_USER_UID)) {
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

	if (DIFFERENT(ru, RES_USER_GID)) {
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

	if (DIFFERENT(ru, RES_USER_GECOS)) {
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

	if (DIFFERENT(ru, RES_USER_DIR)) {
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

	if (DIFFERENT(ru, RES_USER_MKHOME)) {
		action = string("create home directory %s", ru->ru_dir);

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			/* mkdir $home; populate from $skel */
			if (mkdir(ru->ru_dir, 0700) == 0) {
				chown(ru->ru_dir, ru->ru_pw->pw_uid, ru->ru_pw->pw_gid);

				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
			}
		}

		if (ru->ru_skel) {
			action = string("populate home directory from %s", ru->ru_skel);

			if (dryrun) {
				report_action(report, string("populate home directory from %s", ru->ru_skel), ACTION_SKIPPED);
			} else {
				/* copy *all* files from ru_skel into ru_dir */
				if (_res_user_populate_home(ru->ru_dir, ru->ru_skel, ru->ru_pw->pw_uid, ru->ru_pw->pw_gid) == 0) {
					report_action(report, action, ACTION_SUCCEEDED);
				} else {
					report_action(report, action, ACTION_FAILED);
				}
			}
		}
	}

	if (DIFFERENT(ru, RES_USER_SHELL)) {
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

	if (DIFFERENT(ru, RES_USER_PWMIN)) {
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

	if (DIFFERENT(ru, RES_USER_PWMAX)) {
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

	if (DIFFERENT(ru, RES_USER_PWWARN)) {
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

	if (DIFFERENT(ru, RES_USER_INACT)) {
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

	if (DIFFERENT(ru, RES_USER_EXPIRE)) {
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

	if (DIFFERENT(ru, RES_USER_LOCK)) {
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

#define PACK_FORMAT "aLaaLLaaaCaCLLLLL"
char* res_user_pack(const void *res)
{
	const struct res_user *ru = (const struct res_user*)(res);
	assert(ru);

	return pack("res_user::", PACK_FORMAT,
	            ru->key, ru->enforced,
	            ru->ru_name,   ru->ru_passwd, ru->ru_uid,    ru->ru_gid,
	            ru->ru_gecos,  ru->ru_shell,  ru->ru_dir,    ru->ru_mkhome,
	            ru->ru_skel,   ru->ru_lock,   ru->ru_pwmin,  ru->ru_pwmax,
	            ru->ru_pwwarn, ru->ru_inact,  ru->ru_expire);
}

void* res_user_unpack(const char *packed)
{
	struct res_user *ru = res_user_new(NULL);

	if (unpack(packed, "res_user::", PACK_FORMAT,
		&ru->key, &ru->enforced,
		&ru->ru_name,   &ru->ru_passwd, &ru->ru_uid,    &ru->ru_gid,
		&ru->ru_gecos,  &ru->ru_shell,  &ru->ru_dir,    &ru->ru_mkhome,
		&ru->ru_skel,   &ru->ru_lock,   &ru->ru_pwmin,  &ru->ru_pwmax,
		&ru->ru_pwwarn, &ru->ru_inact,  &ru->ru_expire) != 0) {

		res_user_free(ru);
		return NULL;
	}

	return ru;
}
#undef PACK_FORMAT

int res_user_notify(void *res, const struct resource *dep) { return 0; }



/*****************************************************************/

void* res_file_new(const char *key)
{
	struct res_file *rf;

	rf = xmalloc(sizeof(struct res_file));
	list_init(&rf->res);

	rf->enforced = 0;
	rf->different = 0;
	memset(&rf->rf_stat, 0, sizeof(struct stat));
	rf->rf_exists = 0;

	rf->rf_owner = NULL;
	rf->rf_uid = 0;

	rf->rf_group = NULL;
	rf->rf_gid = 0;

	rf->rf_mode = 0600; /* sane default... */

	rf->rf_lpath = NULL;
	rf->rf_rpath = NULL;

	sha1_init(&(rf->rf_lsha1), NULL);
	sha1_init(&(rf->rf_rsha1), NULL);

	rf->key = NULL;

	if (key) {
		rf->key = strdup(key);
		res_file_set(rf, "path", key);
	}

	return rf;
}

void res_file_free(void *res)
{
	struct res_file *rf = (struct res_file*)(res);
	if (rf) {
		list_del(&rf->res);

		free(rf->rf_rpath);
		free(rf->rf_lpath);

		free(rf->key);
	}

	free(rf);
}

char* res_file_key(const void *res)
{
	const struct res_file *rf = (struct res_file*)(res);
	assert(rf);

	return string("res_file:%s", rf->key);
}

int res_file_norm(void *res, struct policy *pol) {
	struct res_file *rf = (struct res_file*)(res);
	assert(rf);

	struct dependency *dep;
	struct resource *other;
	char *key = res_file_key(rf);

	/* files depend on their owner and group */
	if (ENFORCED(rf, RES_FILE_UID)) {
		other = policy_find_resource(pol, RES_USER, "username", rf->rf_owner);

		if (other) {
			dep = dependency_new(key, other->key);
			if (policy_add_dependency(pol, dep) != 0) {
				dependency_free(dep);
				free(key);
				return -1;
			}
		}
	}

	if (ENFORCED(rf, RES_FILE_GID)) {
		other = policy_find_resource(pol, RES_GROUP, "name", rf->rf_group);
		if (other) {
			dep = dependency_new(key, other->key);
			if (policy_add_dependency(pol, dep) != 0) {
				dependency_free(dep);
				free(key);
				return -1;
			}
		}
	}

	free(key);
	return sha1_file(rf->rf_rpath, &rf->rf_rsha1);
}

int res_file_set(void *res, const char *name, const char *value)
{
	struct res_file *rf = (struct res_file*)(res);
	assert(rf);

	if (strcmp(name, "owner") == 0) {
		free(rf->rf_owner);
		rf->rf_owner = strdup(value);
		rf->enforced |= RES_FILE_UID;

	} else if (strcmp(name, "group") == 0) {
		free(rf->rf_group);
		rf->rf_group = strdup(value);
		rf->enforced |= RES_FILE_GID;

	} else if (strcmp(name, "mode") == 0) {
		/* Mask off non-permission bits */
		rf->rf_mode = strtoll(value, NULL, 0) & 07777;
		rf->enforced |= RES_FILE_MODE;

	} else if (strcmp(name, "source") == 0) {
		free(rf->rf_rpath);
		rf->rf_rpath = strdup(value);
		if (sha1_file(rf->rf_rpath, &rf->rf_rsha1) != 0) {
			return -1;
		}
		rf->enforced |= RES_FILE_SHA1;

	} else if (strcmp(name, "path") == 0) {
		free(rf->rf_lpath);
		rf->rf_lpath = strdup(value);

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") != 0) {
			rf->enforced ^= RES_FILE_ABSENT;
		} else {
			rf->enforced |= RES_FILE_ABSENT;
		}

	} else {
		/* unknown attribute. */
		return -1;
	}

	return 0;
}

int res_file_match(const void *res, const char *name, const char *value)
{
	const struct res_file *rf = (struct res_file*)(res);
	assert(rf);

	char *test_value;
	int rc;

	if (strcmp(name, "path") == 0) {
		test_value = string("%s", rf->rf_lpath);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

/*
 * Fill in the local details of res_file structure,
 * including invoking stat(2)
 */
int res_file_stat(void *res, const struct resource_env *env)
{
	struct res_file *rf = (struct res_file*)(res);

	assert(rf);
	assert(rf->rf_lpath);

	if (stat(rf->rf_lpath, &rf->rf_stat) == -1) { /* new file */
		rf->different = rf->enforced;
		rf->rf_exists = 0;
		return 0;
	}

	rf->rf_exists = 1;

	/* only generate sha1 checksums if necessary */
	if (ENFORCED(rf, RES_FILE_SHA1)) {
		if (sha1_file(rf->rf_lpath, &(rf->rf_lsha1)) == -1) {
			return -1;
		}
	}

	rf->different = RES_FILE_NONE;

	if (ENFORCED(rf, RES_FILE_UID) && rf->rf_uid != rf->rf_stat.st_uid) {
		rf->different |= RES_FILE_UID;
	}
	if (ENFORCED(rf, RES_FILE_GID) && rf->rf_gid != rf->rf_stat.st_gid) {
		rf->different |= RES_FILE_GID;
	}
	if (ENFORCED(rf, RES_FILE_MODE) && (rf->rf_stat.st_mode & 07777) != rf->rf_mode) {
		rf->different |= RES_FILE_MODE;
	}
	if (ENFORCED(rf, RES_FILE_SHA1) && memcmp(rf->rf_rsha1.raw, rf->rf_lsha1.raw, SHA1_DIGEST_SIZE) != 0) {
		rf->different |= RES_FILE_SHA1;
	}

	return 0;
}

struct report* res_file_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_file *rf = (struct res_file*)(res);
	assert(rf);
	assert(env);

	struct report *report = report_new("File", rf->rf_lpath);
	char *action;
	int new_file = 0;
	int local_fd = -1;

	/* Remove the file */
	if (ENFORCED(rf, RES_FILE_ABSENT)) {
		if (rf->rf_exists == 1) {
			action = string("remove file");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
			} else if (unlink(rf->rf_lpath) == 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
			}
		}

		return report;
	}

	if (!rf->rf_exists) {
		new_file = 1;
		action = string("create file");

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			local_fd = creat(rf->rf_lpath, rf->rf_mode);
			if (local_fd >= 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
				return report;
			}
		}

		rf->different = rf->enforced;
		/* No need to chmod the file again */
		rf->different ^= RES_FILE_MODE;
	}

	if (DIFFERENT(rf, RES_FILE_SHA1)) {
		assert(rf->rf_lpath);

		action = string("update content from master copy");
		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			if (local_fd < 0) {
				local_fd = open(rf->rf_lpath, O_CREAT | O_RDWR | O_TRUNC, rf->rf_mode);
				if (local_fd == -1) {
					report_action(report, action, ACTION_FAILED);
					return report;
				}
			}

			if (env->file_fd == -1) {
				report_action(report, action, ACTION_FAILED);
				return report;
			}

			if (_res_file_fd2fd(local_fd, env->file_fd, env->file_len) == -1) {
				report_action(report, action, ACTION_FAILED);
				return report;
			}

			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (DIFFERENT(rf, RES_FILE_UID)) {
		if (new_file) {
			action = string("set owner to %s(%u)", rf->rf_owner, rf->rf_uid);
		} else {
			action = string("change owner from %u to %s(%u)", rf->rf_stat.st_uid, rf->rf_owner, rf->rf_uid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (chown(rf->rf_lpath, rf->rf_uid, -1) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	if (DIFFERENT(rf, RES_FILE_GID)) {
		if (new_file) {
			action = string("set group to %s(%u)", rf->rf_group, rf->rf_gid);
		} else {
			action = string("change group from %u to %s(%u)", rf->rf_stat.st_gid, rf->rf_group, rf->rf_gid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (chown(rf->rf_lpath, -1, rf->rf_gid) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	if (DIFFERENT(rf, RES_FILE_MODE)) {
		if (new_file) {
			action = string("set permissions to %04o", rf->rf_mode);
		} else {
			action = string("change permissions from %04o to %04o", rf->rf_stat.st_mode & 07777, rf->rf_mode);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (chmod(rf->rf_lpath, rf->rf_mode) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	return report;
}

#define PACK_FORMAT "aLaaaaL"
char* res_file_pack(const void *res)
{
	const struct res_file *rf = (const struct res_file*)(res);
	assert(rf);

	return pack("res_file::", PACK_FORMAT,
	            rf->key, rf->enforced,
	            rf->rf_lpath, rf->rf_rsha1.hex, rf->rf_owner, rf->rf_group, rf->rf_mode);
}

void* res_file_unpack(const char *packed)
{
	char *hex = NULL;
	struct res_file *rf = res_file_new(NULL);

	if (unpack(packed, "res_file::", PACK_FORMAT,
		&rf->key, &rf->enforced,
		&rf->rf_lpath, &hex, &rf->rf_owner, &rf->rf_group, &rf->rf_mode) != 0) {

		free(hex);
		res_file_free(rf);
		return NULL;
	}

	sha1_init(&rf->rf_rsha1, hex);
	free(hex);

	return rf;
}
#undef PACK_FORMAT

int res_file_notify(void *res, const struct resource *dep) { return 0; }

/*****************************************************************/

void* res_group_new(const char *key)
{
	struct res_group *rg;

	rg = xmalloc(sizeof(struct res_group));
	list_init(&rg->res);

	rg->rg_name = NULL;
	rg->rg_passwd = NULL;
	rg->rg_gid = 0;

	rg->rg_grp = NULL;
	rg->rg_sg  = NULL;

	rg->rg_mem = NULL;
	rg->rg_mem_add = stringlist_new(NULL);
	rg->rg_mem_rm  = stringlist_new(NULL);

	rg->rg_adm = NULL;
	rg->rg_adm_add = stringlist_new(NULL);
	rg->rg_adm_rm  = stringlist_new(NULL);

	rg->enforced  = RES_GROUP_NONE;
	rg->different = RES_GROUP_NONE;

	if (key) {
		rg->key = strdup(key);
		res_group_set(rg, "name", key);
	} else {
		rg->key = NULL;
	}

	return rg;
}

void res_group_free(void *res)
{
	struct res_group *rg = (struct res_group*)(res);

	if (rg) {
		free(rg->key);

		free(rg->rg_name);
		free(rg->rg_passwd);

		list_del(&rg->res);

		if (rg->rg_mem) {
			stringlist_free(rg->rg_mem);
		}
		stringlist_free(rg->rg_mem_add);
		stringlist_free(rg->rg_mem_rm);

		if (rg->rg_adm) {
			stringlist_free(rg->rg_adm);
		}
		stringlist_free(rg->rg_adm_add);
		stringlist_free(rg->rg_adm_rm);
	}
	free(rg);
}

char* res_group_key(const void *res)
{
	const struct res_group *rg = (struct res_group*)(res);
	assert(rg);

	return string("res_group:%s", rg->key);
}

int res_group_norm(void *res, struct policy *pol) { return 0; }

int res_group_set(void *res, const char *name, const char *value)
{
	struct res_group *rg = (struct res_group*)(res);
	assert(rg);

	if (strcmp(name, "gid") == 0) {
		rg->rg_gid = strtoll(value, NULL, 10);
		rg->enforced |= RES_GROUP_GID;

	} else if (strcmp(name, "name") == 0) {
		free(rg->rg_name);
		rg->rg_name = strdup(value);
		rg->enforced |= RES_GROUP_NAME;

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") != 0) {
			rg->enforced ^= RES_GROUP_ABSENT;
		} else {
			rg->enforced |= RES_GROUP_ABSENT;
		}

	} else if (strcmp(name, "member") == 0) {
		if (value[0] == '!') {
			return res_group_remove_member(rg, value+1);
		} else {
			return res_group_add_member(rg, value);
		}
	} else if (strcmp(name, "admin") == 0) {
		if (value[0] == '!') {
			return res_group_remove_admin(rg, value+1);
		} else {
			return res_group_add_admin(rg, value);
		}
	} else if (strcmp(name, "pwhash") == 0 || strcmp(name, "password") == 0) {
		free(rg->rg_passwd);
		rg->rg_passwd = strdup(value);
		rg->enforced |= RES_GROUP_PASSWD;

	} else {
		return -1;
	}

	return 0;
}

int res_group_match(const void *res, const char *name, const char *value)
{
	const struct res_group *rg = (struct res_group*)(res);
	assert(rg);

	char *test_value;
	int rc;

	if (strcmp(name, "gid") == 0) {
		test_value = string("%u", rg->rg_gid);
	} else if (strcmp(name, "name") == 0) {
		test_value = string("%s", rg->rg_name);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_group_enforce_members(struct res_group *rg, int enforce)
{
	assert(rg);

	if (enforce) {
		rg->enforced |= RES_GROUP_MEMBERS;
	} else {
		rg->enforced ^= RES_GROUP_MEMBERS;
	}
	return 0;
}

/* updates rg_mem_add */
int res_group_add_member(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

	res_group_enforce_members(rg, 1);
	/* add to rg_mem_add, remove from rg_mem_rm */
	return _group_update(rg->rg_mem_add, rg->rg_mem_rm, user);
}
/* updates rg_mem_rm */
int res_group_remove_member(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

	res_group_enforce_members(rg, 1);
	/* add to rg_mem_rm, remove from rg_mem_add */
	return _group_update(rg->rg_mem_rm, rg->rg_mem_add, user);
}

int res_group_enforce_admins(struct res_group *rg, int enforce)
{
	assert(rg);

	if (enforce) {
		rg->enforced |= RES_GROUP_ADMINS;
	} else {
		rg->enforced ^= RES_GROUP_ADMINS;
	}
	return 0;
}

/* updates rg_adm_add */
int res_group_add_admin(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

	res_group_enforce_admins(rg, 1);
	/* add to rg_adm_add, remove from rg_adm_rm */
	return _group_update(rg->rg_adm_add, rg->rg_adm_rm, user);
}

/* updates rg_adm_rm */
int res_group_remove_admin(struct res_group *rg, const char *user)
{
	assert(rg);
	assert(user);

	res_group_enforce_admins(rg, 1);
	/* add to rg_adm_rm, remove from rg_adm_add */
	return _group_update(rg->rg_adm_rm, rg->rg_adm_add, user);
}

int res_group_stat(void *res, const struct resource_env *env)
{
	struct res_group *rg = (struct res_group*)(res);
	stringlist *list;

	assert(rg);
	assert(env);
	assert(env->group_grdb);
	assert(env->group_sgdb);

	rg->rg_grp = grdb_get_by_name(env->group_grdb, rg->rg_name);
	rg->rg_sg = sgdb_get_by_name(env->group_sgdb, rg->rg_name);
	if (!rg->rg_grp || !rg->rg_sg) { /* new group */
		rg->different = rg->enforced;

		rg->rg_mem = stringlist_new(NULL);
		stringlist_add_all(rg->rg_mem, rg->rg_mem_add);

		rg->rg_adm = stringlist_new(NULL);
		stringlist_add_all(rg->rg_adm, rg->rg_adm_add);

		return 0;
	}

	/* set up rg_mem as the list we want for gr_mem */
	rg->rg_mem = stringlist_new(rg->rg_grp->gr_mem);
	stringlist_add_all(rg->rg_mem, rg->rg_mem_add);
	stringlist_remove_all(rg->rg_mem, rg->rg_mem_rm);
	stringlist_uniq(rg->rg_mem);

	/* set up rg_adm as the list we want for sg_adm */
	rg->rg_adm = stringlist_new(rg->rg_sg->sg_adm);
	stringlist_add_all(rg->rg_adm, rg->rg_adm_add);
	stringlist_remove_all(rg->rg_adm, rg->rg_adm_rm);
	stringlist_uniq(rg->rg_adm);

	rg->different = RES_GROUP_NONE;

	if (ENFORCED(rg, RES_GROUP_NAME) && strcmp(rg->rg_name, rg->rg_grp->gr_name) != 0) {
		rg->different |= RES_GROUP_NAME;
	}

	if (ENFORCED(rg, RES_GROUP_PASSWD) && strcmp(rg->rg_passwd, rg->rg_sg->sg_passwd) != 0) {
		rg->different |= RES_GROUP_PASSWD;
	}

	if (ENFORCED(rg, RES_GROUP_GID) && rg->rg_gid != rg->rg_grp->gr_gid) {
		rg->different |= RES_GROUP_GID;
	}

	if (ENFORCED(rg, RES_GROUP_MEMBERS)) {
		/* use list as a stringlist of gr_mem */
		list = stringlist_new(rg->rg_grp->gr_mem);
		if (stringlist_diff(list, rg->rg_mem) == 0) {
			rg->different |= RES_GROUP_MEMBERS;
		}
		stringlist_free(list);
	}

	if (ENFORCED(rg, RES_GROUP_ADMINS)) {
		/* use list as a stringlist of sg_adm */
		list = stringlist_new(rg->rg_sg->sg_adm);
		if (stringlist_diff(list, rg->rg_adm) == 0) {
			rg->different |= RES_GROUP_ADMINS;
		}
		stringlist_free(list);
	}

	return 0;
}

struct report* res_group_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_group *rg = (struct res_group*)(res);
	assert(rg);
	assert(env);
	assert(env->group_grdb);
	assert(env->group_sgdb);

	struct report *report;
	char *action;

	int new_group;
	stringlist *orig;
	stringlist *to_add;
	stringlist *to_remove;

	report = report_new("Group", rg->rg_name);

	/* Remove the group if RES_GROUP_ABSENT */
	if (ENFORCED(rg, RES_GROUP_ABSENT)) {
		if (rg->rg_grp || rg->rg_sg) {
			action = string("remove group");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
			} else {
				if ((rg->rg_grp && grdb_rm(env->group_grdb, rg->rg_grp) != 0)
				 || (rg->rg_sg && sgdb_rm(env->group_sgdb, rg->rg_sg) != 0)) {
					report_action(report, action, ACTION_FAILED);
				} else {
					report_action(report, action, ACTION_SUCCEEDED);
				}
			}
		}

		return report;
	}

	if (!rg->rg_grp || !rg->rg_sg) {
		new_group = 1;
		action = string("create group");

		if (!rg->rg_grp) { rg->rg_grp = grdb_new_entry(env->group_grdb, rg->rg_name, rg->rg_gid); }
		if (!rg->rg_sg)  { rg->rg_sg = sgdb_new_entry(env->group_sgdb, rg->rg_name); }

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (rg->rg_grp && rg->rg_sg) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	if (DIFFERENT(rg, RES_GROUP_PASSWD)) {
		if (new_group) {
			action = string("set group membership password");
		} else {
			action = string("change group membership password");
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			xfree(rg->rg_grp->gr_passwd);
			rg->rg_grp->gr_passwd = strdup("x");
			xfree(rg->rg_sg->sg_passwd);
			rg->rg_sg->sg_passwd = strdup(rg->rg_passwd);

			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (DIFFERENT(rg, RES_GROUP_GID)) {
		if (new_group) {
			action = string("set gid to %u", rg->rg_gid);
		} else {
			action = string("change gid from %u to %u", rg->rg_grp->gr_gid, rg->rg_gid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			rg->rg_grp->gr_gid = rg->rg_gid;
			report_action(report, action, ACTION_SUCCEEDED);
		}
	}

	if (ENFORCED(rg, RES_GROUP_MEMBERS) && DIFFERENT(rg, RES_GROUP_MEMBERS)) {
		orig = stringlist_new(rg->rg_grp->gr_mem);
		to_add = stringlist_dup(rg->rg_mem_add);
		stringlist_remove_all(to_add, orig);

		to_remove = stringlist_intersect(orig, rg->rg_mem_rm);

		if (!dryrun) {
			/* replace gr_mem and sg_mem with the rg_mem stringlist */
			xarrfree(rg->rg_grp->gr_mem);
			rg->rg_grp->gr_mem = xarrdup(rg->rg_mem->strings);

			xarrfree(rg->rg_sg->sg_mem);
			rg->rg_sg->sg_mem = xarrdup(rg->rg_mem->strings);
		}

		size_t i;
		for (i = 0; i < to_add->num; i++) {
			report_action(report, string("add %s", to_add->strings[i]), (dryrun ? ACTION_SKIPPED : ACTION_SUCCEEDED));
		}
		for (i = 0; i < to_remove->num; i++) {
			report_action(report, string("remove %s", to_remove->strings[i]), (dryrun ? ACTION_SKIPPED : ACTION_SUCCEEDED));
		}

		stringlist_free(orig);
		stringlist_free(to_add);
		stringlist_free(to_remove);
	}

	if (ENFORCED(rg, RES_GROUP_ADMINS) && DIFFERENT(rg, RES_GROUP_ADMINS)) {
		orig = stringlist_new(rg->rg_sg->sg_adm);
		to_add = stringlist_dup(rg->rg_adm_add);
		stringlist_remove_all(to_add, orig);

		to_remove = stringlist_intersect(orig, rg->rg_mem_add);

		if (!dryrun) {
			/* replace sg_adm with the rg_adm stringlist */
			xarrfree(rg->rg_sg->sg_adm);
			rg->rg_sg->sg_adm = xarrdup(rg->rg_adm->strings);
		}

		size_t i;
		for (i = 0; i < to_add->num; i++) {
			report_action(report, string("grant admin rights to %s", to_add->strings[i]), (dryrun ? ACTION_SKIPPED : ACTION_SUCCEEDED));
		}
		for (i = 0; i < to_remove->num; i++) {
			report_action(report, string("revoke admin rights from %s", to_remove->strings[i]), (dryrun ? ACTION_SKIPPED : ACTION_SUCCEEDED));
		}

		stringlist_free(orig);
		stringlist_free(to_add);
		stringlist_free(to_remove);
	}

	return report;
}

#define PACK_FORMAT "aLaaLaaaa"
char *res_group_pack(const void *res)
{
	const struct res_group *rg = (const struct res_group*)(res);
	assert(rg);

	char *tmp;
	char *mem_add = NULL, *mem_rm = NULL,
	     *adm_add = NULL, *adm_rm = NULL;

	if (!(mem_add = stringlist_join(rg->rg_mem_add, "."))
	 || !(mem_rm  = stringlist_join(rg->rg_mem_rm,  "."))
	 || !(adm_add = stringlist_join(rg->rg_adm_add, "."))
	 || !(adm_rm  = stringlist_join(rg->rg_adm_rm,  "."))) {
		free(mem_add); free(mem_rm);
		free(adm_add); free(adm_rm);
		return NULL;
	}

	tmp = pack("res_group::", PACK_FORMAT,
	           rg->key, rg->enforced,
	           rg->rg_name, rg->rg_passwd, rg->rg_gid,
	           mem_add, mem_rm, adm_add, adm_rm);

	free(mem_add); free(mem_rm);
	free(adm_add); free(adm_rm);

	return tmp;
}

void* res_group_unpack(const char *packed)
{
	char *mem_add = NULL, *mem_rm = NULL,
	     *adm_add = NULL, *adm_rm = NULL;
	struct res_group *rg = res_group_new(NULL);

	if (unpack(packed, "res_group::", PACK_FORMAT,
		&rg->key, &rg->enforced,
		&rg->rg_name, &rg->rg_passwd, &rg->rg_gid,
		&mem_add, &mem_rm, &adm_add, &adm_rm)) {

		res_group_free(rg);
		return NULL;
	}

	stringlist_free(rg->rg_mem_add);
	rg->rg_mem_add = stringlist_split(mem_add, strlen(mem_add), ".");

	stringlist_free(rg->rg_mem_rm);
	rg->rg_mem_rm  = stringlist_split(mem_rm,  strlen(mem_rm),  ".");

	stringlist_free(rg->rg_adm_add);
	rg->rg_adm_add = stringlist_split(adm_add, strlen(adm_add), ".");

	stringlist_free(rg->rg_adm_rm);
	rg->rg_adm_rm  = stringlist_split(adm_rm,  strlen(adm_rm),  ".");

	free(mem_add); free(mem_rm);
	free(adm_add); free(adm_rm);

	return rg;
}
#undef PACK_FORMAT

int res_group_notify(void *res, const struct resource *dep) { return 0; }


void* res_package_new(const char *key)
{
	struct res_package *rp;

	rp = xmalloc(sizeof(struct res_package));
	list_init(&rp->res);

	rp->enforced = 0;
	rp->different = 0;
	rp->version = NULL;

	if (key) {
		rp->key = strdup(key);
		res_package_set(rp, "name", key);
	} else {
		rp->key = NULL;
	}

	return rp;
}

void res_package_free(void *res)
{
	struct res_package *rp = (struct res_package*)(res);
	if (rp) {
		list_del(&rp->res);

		free(rp->name);
		free(rp->version);

		free(rp->key);
	}

	free(rp);
}

char* res_package_key(const void *res)
{
	const struct res_package *rp = (struct res_package*)(res);
	assert(rp);

	return string("res_package:%s", rp->key);
}

int res_package_norm(void *res, struct policy *pol) { return 0; }

int res_package_set(void *res, const char *name, const char *value)
{
	struct res_package *rp = (struct res_package*)(res);
	assert(rp);

	if (strcmp(name, "name") == 0) {
		free(rp->name);
		rp->name = strdup(value);

	} else if (strcmp(name, "version") == 0) {
		free(rp->version);
		rp->version = strdup(value);

	} else if (strcmp(name, "installed") == 0) {
		if (strcmp(value, "no") != 0) {
			rp->enforced ^= RES_PACKAGE_ABSENT;
		} else {
			rp->enforced |= RES_PACKAGE_ABSENT;
		}

	} else {
		return -1;
	}

	return 0;
}

int res_package_match(const void *res, const char *name, const char *value)
{
	const struct res_package *rp = (const struct res_package*)(res);
	assert(rp);

	char *test_value;
	int rc;

	if (strcmp(name, "name") == 0) {
		test_value = string("%s", name);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_package_stat(void *res, const struct resource_env *env)
{
	struct res_package *rp = (struct res_package*)(res);

	assert(rp);
	assert(env);
	assert(env->package_manager);

	free(rp->installed);
	rp->installed = package_version(env->package_manager, rp->name);

	return 0;
}

struct report* res_package_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_package *rp = (struct res_package*)(res);

	assert(rp);
	assert(env);
	assert(env->package_manager);

	struct report *report = report_new("Package", rp->name);
	char *action;

	if (ENFORCED(rp, RES_PACKAGE_ABSENT)) {
		if (rp->installed) {
			action = string("uninstall package");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
			} else if (package_remove(env->package_manager, rp->name) == 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
			}
		}

		return report;
	}

	if (!rp->installed) {
		action = (rp->version ? string("install package v%s", rp->version)
		                      : string("install package (latest version)"));

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (package_install(env->package_manager, rp->name, rp->version) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}

		return report;
	}

	if (rp->version && strcmp(rp->installed, rp->version) != 0) {
		action = string("upgrade to v%s", rp->version);

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (package_install(env->package_manager, rp->name, rp->version) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}

		return report;
	}

	return report;
}

#define PACK_FORMAT "aLaa"
char* res_package_pack(const void *res)
{
	const struct res_package *rp = (const struct res_package*)(res);
	assert(rp);

	return pack("res_package::", PACK_FORMAT,
	            rp->key, rp->enforced, rp->name, rp->version);
}

void* res_package_unpack(const char *packed)
{
	struct res_package *rp = res_package_new(NULL);

	if (unpack(packed, "res_package::", PACK_FORMAT,
		&rp->key, &rp->enforced, &rp->name, &rp->version) != 0) {

		res_package_free(rp);
		return NULL;
	}

	if (rp->version && !*(rp->version)) {
		/* treat "" as NULL */
		free(rp->version);
		rp->version = NULL;
	}

	return rp;
}
#undef PACK_FORMAT

int res_package_notify(void *res, const struct resource *dep) { return 0; }


void* res_service_new(const char *key)
{
	struct res_service *rs;

	rs = xmalloc(sizeof(struct res_service));
	list_init(&rs->res);

	rs->enforced = 0;
	rs->different = 0;

	rs->key = NULL;
	if (key) {
		rs->key = strdup(key);
		res_service_set(rs, "service", key);
	}

	return rs;
}

void res_service_free(void *res)
{
	struct res_service *rs = (struct res_service*)(res);
	if (rs) {
		list_del(&rs->res);

		free(rs->service);

		free(rs->key);
	}

	free(rs);
}

char* res_service_key(const void *res)
{
	const struct res_service *rs = (struct res_service*)(res);
	assert(rs);

	return string("res_service:%s", rs->key);
}

int res_service_norm(void *res, struct policy *pol) { return 0; }

int res_service_set(void *res, const char *name, const char *value)
{
	struct res_service *rs = (struct res_service*)(res);
	assert(rs);

	if (strcmp(name, "name") == 0 || strcmp(name, "service") == 0) {
		free(rs->service);
		rs->service = strdup(value);

	} else if (strcmp(name, "running") == 0) {
		if (strcmp(value, "no") != 0) {
			rs->enforced ^= RES_SERVICE_STOPPED;
			rs->enforced |= RES_SERVICE_RUNNING;
		} else {
			rs->enforced ^= RES_SERVICE_RUNNING;
			rs->enforced |= RES_SERVICE_STOPPED;
		}

	} else if (strcmp(name, "stopped") == 0) {
		if (strcmp(value, "no") != 0) {
			rs->enforced ^= RES_SERVICE_RUNNING;
			rs->enforced |= RES_SERVICE_STOPPED;
		} else {
			rs->enforced ^= RES_SERVICE_STOPPED;
			rs->enforced |= RES_SERVICE_RUNNING;
		}

	} else if (strcmp(name, "enabled") == 0) {
		if (strcmp(value, "no") != 0) {
			rs->enforced ^= RES_SERVICE_DISABLED;
			rs->enforced |= RES_SERVICE_ENABLED;
		} else {
			rs->enforced ^= RES_SERVICE_ENABLED;
			rs->enforced |= RES_SERVICE_DISABLED;
		}

	} else if (strcmp(name, "disabled") == 0) {
		if (strcmp(value, "no") != 0) {
			rs->enforced ^= RES_SERVICE_ENABLED;
			rs->enforced |= RES_SERVICE_DISABLED;
		} else {
			rs->enforced ^= RES_SERVICE_DISABLED;
			rs->enforced |= RES_SERVICE_ENABLED;
		}

	} else {
		return -1;
	}

	return 0;
}

int res_service_match(const void *res, const char *name, const char *value)
{
	const struct res_service *rs = (const struct res_service*)(res);
	assert(rs);

	char *test_value;
	int rc;

	if (strcmp(name, "name") == 0 || strcmp(name, "service") == 0) {
		test_value = string("%s", rs->service);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_service_stat(void *res, const struct resource_env *env)
{
	struct res_service *rs = (struct res_service*)(res);

	assert(rs);
	assert(env);
	assert(env->service_manager);

	rs->enabled = (service_enabled(env->service_manager, rs->service) == 0 ? 1 : 0);
	rs->running = (service_running(env->service_manager, rs->service) == 0 ? 1 : 0);

	return 0;
}

struct report* res_service_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_service *rs = (struct res_service*)(res);

	assert(rs);
	assert(env);
	assert(env->service_manager);

	struct report *report = report_new("Service", rs->service);
	char *action;

	if (ENFORCED(rs, RES_SERVICE_ENABLED) && !rs->enabled) {
		action = string("enable service");

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (service_enable(env->service_manager, rs->service) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}

	} else if (ENFORCED(rs, RES_SERVICE_DISABLED) && rs->enabled) {
		action = string("disable service");

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (service_disable(env->service_manager, rs->service) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	if (ENFORCED(rs, RES_SERVICE_RUNNING) && !rs->running) {
		action = string("start service");

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (service_start(env->service_manager, rs->service) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}

	} else if (ENFORCED(rs, RES_SERVICE_STOPPED) && rs->running) {
		action = string("stop service");

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (service_stop(env->service_manager, rs->service) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	} else if (rs->running && rs->notified) {
		action = string("reload service (via dependency)");

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (service_reload(env->service_manager, rs->service) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	rs->notified = 0;

	return report;
}

#define PACK_FORMAT "aLa"
char* res_service_pack(const void *res)
{
	const struct res_service *rs = (const struct res_service*)(res);
	assert(rs);

	return pack("res_service::", PACK_FORMAT,
	            rs->key, rs->enforced, rs->service);
}

void* res_service_unpack(const char *packed)
{
	struct res_service *rs = res_service_new(NULL);

	if (unpack(packed, "res_service::", PACK_FORMAT,
		&rs->key, &rs->enforced, &rs->service) != 0) {

		res_service_free(rs);
		return NULL;
	}

	return rs;
}
#undef PACK_FORMAT

int res_service_notify(void *res, const struct resource *dep)
{
	struct res_service *rs = (struct res_service*)(res);
	assert(rs);

	rs->notified = 1;

	return 0;
}
