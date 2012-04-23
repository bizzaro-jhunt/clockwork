/*
  Copyright 2011 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "resources.h"
#include "template.h"
#include "augcw.h"

#include <fts.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/wait.h>
#include <time.h>

#define ENFORCE(r,f)   (r)->enforced  |=  (f)
#define UNENFORCE(r,f) (r)->enforced  &= ~(f)
#define DIFF(r,f)      (r)->different |=  (f)
#define UNDIFF(r,f)    (r)->different &= ~(f)

#define _FD2FD_CHUNKSIZE 16384

static int _res_user_populate_home(const char *home, const char *skel, uid_t uid, gid_t gid);
static int _res_file_gen_rsha1(struct res_file *rf, struct hash *facts);
static int _res_file_fd2fd(int dest, int src, ssize_t bytes);
static int _group_update(struct stringlist*, struct stringlist*, const char*);
static char* _sysctl_path(const char *param);
static int _sysctl_read(const char *param, char **value);
static int _sysctl_write(const char *param, const char *value);
static int _mkdir_p(const char *path);
static int _mkdir_c(const char *path);
static int _setup_path_deps(const char *key, const char *path, struct policy *pol);

#define RES_DEFAULT(orig,field,dflt) ((orig) ? (orig)->field : (dflt))
#define RES_DEFAULT_STR(orig,field,dflt) xstrdup(RES_DEFAULT((orig),field,(dflt)))

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

				if (chown(home_path, uid, gid) != 0) {
					_res_file_fd2fd(home_fd, skel_fd, -1);
				}
				if (skel_fd >= 0 && home_fd >= 0
				 && chown(home_path, uid, gid) == 0) {
					while ((nread = read(skel_fd, buf, 8192)) > 0)
						write(home_fd, buf, nread);
				}

			} else if (S_ISDIR(ent->fts_statp->st_mode)) {
				if (mkdir(home_path, mode) == 0
				 && chown(home_path, uid, gid) == 0) {
					chmod(home_path, 0755);
				}
			}

			free(home_path);
			free(skel_path);
		}
	}

	free(path_argv[0]);
	fts_close(fts);
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

static int _res_file_gen_rsha1(struct res_file *rf, struct hash *facts)
{
	assert(rf); // LCOV_EXCL_LINE

	int rc;
	char *contents = NULL;
	struct template *t = NULL;


	if (rf->rf_rpath) {
		return sha1_file(rf->rf_rpath, &rf->rf_rsha1);

	} else if (rf->rf_template) {
		t = template_create(rf->rf_template, facts);
		contents = template_render(t);

		rc = sha1_data(contents, strlen(contents), &rf->rf_rsha1);

		template_free(t);
		free(contents);
		return rc;
	}

	return 0;
}

static int _group_update(struct stringlist *add, struct stringlist *rm, const char *user)
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

static char* _sysctl_path(const char *param)
{
	char *path, *p;

	path = string("/proc/sys/%s", param);
	for (p = path; *p; p++) {
		if (*p == '.') {
			*p = '/';
		}
	}

	return path;
}

static int _sysctl_read(const char *param, char **value)
{
	char *path;
	char *p, buf[256] = {0};
	int fd;

	path = _sysctl_path(param);
	fd = open(path, O_RDONLY);
	free(path);

	if (fd < 0 || read(fd, buf, 255) < 0) {
		return -1;
	}

	if (value) {
		for (p = buf; *p; p++) {
			if (*p == '\t') { *p = ' '; }
			if (*p == '\n') { *p = '\0'; }
		}

		*value = strdup(buf);
	}
	return 0;
}

static int _sysctl_write(const char *param, const char *value)
{
	char *path;
	int fd;
	size_t len;
	ssize_t n, nwritten;

	path = _sysctl_path(param);
	DEBUG("sysctl: Writing value to %s", path);
	fd = open(path, O_WRONLY);
	free(path);

	if (fd < 0) {
		return -1;
	}

	len = strlen(value);
	nwritten = 0;
	do {
		n = write(fd, value + nwritten, len - nwritten);
		DEBUG("%i/%u bytes written; n = %i", nwritten, len, n);
		if (n <= 0) { return n; }
		nwritten += n;
	} while (nwritten < len);

	return nwritten;
}

static int _mkdir_p(const char *s)
{
	struct path *p;
	struct stat st;
	memset(&st, 0, sizeof(struct stat));

	p = path_new(s);
	if (path_canon(p) != 0) { goto failed; }

	do {
		errno = 0;
		if (stat(path(p), &st) == 0) { break; }
		if (errno != ENOENT) { goto failed; }

		DEBUG("mkdir: %s does not exist...", path(p));
	} while (path_pop(p) != 0);

	while (path_push(p) != 0) {
		DEBUG("mkdir: creating %s", path(p));
		if (mkdir(path(p), 0755) != 0) { goto failed; }
	}

	path_free(p);
	return 0;

failed:
	path_free(p);
	return -1;
}

static int _mkdir_c(const char *s)
{
	char *copy = strdup(s);
	char *dir = dirname(copy);
	int rc;

	DEBUG("mkdir_c: passing %s to _mkdir_p", dir);
	rc =_mkdir_p(dir);

	free(copy);
	return rc;
}

static int _setup_path_deps(const char *key, const char *spath, struct policy *pol)
{
	struct path *p;
	struct dependency *dep;
	struct resource *dir;

	DEBUG("setup_path_deps: setting up for %s (%s)", spath, key);
	p = path_new(spath);
	if (!p) {
		return -1;
	}
	if (path_canon(p) != 0) { goto failed; }

	while (path_pop(p) != 0) {
		/* look for deps, and add them. */
		DEBUG("setup_path_deps: looking for %s", path(p));
		dir = policy_find_resource(pol, RES_DIR, "path", path(p));
		if (dir) {
			dep = dependency_new(key, dir->key);
			if (policy_add_dependency(pol, dep) != 0) {
				dependency_free(dep);
				goto failed;
			}
		} else {
			DEBUG("setup_path_deps: no res_dir defined for '%s'", path(p));
		}
	}

	path_free(p);
	return 0;

failed:
	DEBUG("setup_path_deps: unspecified failure");
	path_free(p);
	return -1;
}


/*****************************************************************/

void* res_user_new(const char *key)
{
	return res_user_clone(NULL, key);
}

void* res_user_clone(const void *res, const char *key)
{
	struct res_user *orig = (struct res_user*)(res);
	struct res_user *ru = ru = xmalloc(sizeof(struct res_user));

	ru->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);
	ru->different = RES_NONE;

	ru->ru_uid    = RES_DEFAULT(orig, ru_uid,    -1);
	ru->ru_gid    = RES_DEFAULT(orig, ru_gid,    -1);
	ru->ru_mkhome = RES_DEFAULT(orig, ru_mkhome,  0);
	ru->ru_lock   = RES_DEFAULT(orig, ru_lock,    1);
	ru->ru_pwmin  = RES_DEFAULT(orig, ru_pwmin,   0);
	ru->ru_pwmax  = RES_DEFAULT(orig, ru_pwmax,   0);
	ru->ru_pwwarn = RES_DEFAULT(orig, ru_pwwarn,  0);
	ru->ru_inact  = RES_DEFAULT(orig, ru_inact,   0);
	ru->ru_expire = RES_DEFAULT(orig, ru_expire, -1);

	ru->ru_name   = RES_DEFAULT_STR(orig, ru_name,   NULL);
	ru->ru_passwd = RES_DEFAULT_STR(orig, ru_passwd, NULL);
	ru->ru_gecos  = RES_DEFAULT_STR(orig, ru_gecos,  NULL);
	ru->ru_shell  = RES_DEFAULT_STR(orig, ru_shell,  NULL);
	ru->ru_dir    = RES_DEFAULT_STR(orig, ru_dir,    NULL);
	ru->ru_skel   = RES_DEFAULT_STR(orig, ru_skel,   NULL);

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
	assert(ru); // LCOV_EXCL_LINE

	return string("user:%s", ru->key);
}

int res_user_attrs(const void *res, struct hash *attrs)
{
	const struct res_user *ru = (const struct res_user*)(res);
	assert(ru); // LCOV_EXCL_LINE

	hash_set(attrs, "username", strdup(ru->ru_name));
	hash_set(attrs, "uid", ENFORCED(ru, RES_USER_UID) ? string("%u",ru->ru_uid) : NULL);
	hash_set(attrs, "gid", ENFORCED(ru, RES_USER_GID) ? string("%u",ru->ru_gid) : NULL);
	hash_set(attrs, "home", ENFORCED(ru, RES_USER_DIR) ? strdup(ru->ru_dir) : NULL);
	hash_set(attrs, "present", strdup(ENFORCED(ru, RES_USER_ABSENT) ? "no" : "yes"));
	hash_set(attrs, "locked", ENFORCED(ru, RES_USER_LOCK) ? strdup(ru->ru_lock ? "yes" : "no") : NULL);
	hash_set(attrs, "comment", ENFORCED(ru, RES_USER_GECOS) ? strdup(ru->ru_gecos) : NULL);
	hash_set(attrs, "shell", ENFORCED(ru, RES_USER_SHELL) ? strdup(ru->ru_shell) : NULL);
	hash_set(attrs, "password", ENFORCED(ru, RES_USER_PASSWD) ? strdup(ru->ru_passwd) : NULL);
	hash_set(attrs, "pwmin", ENFORCED(ru, RES_USER_PWMIN) ? string("%u", ru->ru_pwmin) : NULL);
	hash_set(attrs, "pwmax", ENFORCED(ru, RES_USER_PWMAX) ? string("%u", ru->ru_pwmax) : NULL);
	hash_set(attrs, "pwwarn", ENFORCED(ru, RES_USER_PWWARN) ? string("%u", ru->ru_pwwarn) : NULL);
	hash_set(attrs, "inact", ENFORCED(ru, RES_USER_INACT) ? string("%u", ru->ru_inact) : NULL);
	hash_set(attrs, "expiration", ENFORCED(ru, RES_USER_EXPIRE) ? string("%u", ru->ru_expire) : NULL);
	hash_set(attrs, "skeleton", ENFORCED(ru, RES_USER_MKHOME) ? strdup(ru->ru_skel) : NULL);
	return 0;
}

int res_user_norm(void *res, struct policy *pol, struct hash *facts) { return 0; }

int res_user_set(void *res, const char *name, const char *value)
{
	struct res_user *ru = (struct res_user*)(res);
	assert(ru); // LCOV_EXCL_LINE

	if (strcmp(name, "uid") == 0) {
		ru->ru_uid = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_UID);

	} else if (strcmp(name, "gid") == 0) {
		ru->ru_gid = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_GID);

	} else if (strcmp(name, "username") == 0) {
		free(ru->ru_name);
		ru->ru_name = strdup(value);
		ENFORCE(ru, RES_USER_NAME);

	} else if (strcmp(name, "home") == 0) {
		free(ru->ru_dir);
		ru->ru_dir = strdup(value);
		ENFORCE(ru, RES_USER_DIR);

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(ru, RES_USER_ABSENT);
		} else {
			ENFORCE(ru, RES_USER_ABSENT);
		}

	} else if (strcmp(name, "locked") == 0) {
		ru->ru_lock = strcmp(value, "no") ? 1 : 0;
		ENFORCE(ru, RES_USER_LOCK);

	} else if (strcmp(name, "gecos") == 0 || strcmp(name, "comment") == 0) {
		free(ru->ru_gecos);
		ru->ru_gecos = strdup(value);
		ENFORCE(ru, RES_USER_GECOS);

	} else if (strcmp(name, "shell") == 0) {
		free(ru->ru_shell);
		ru->ru_shell = strdup(value);
		ENFORCE(ru, RES_USER_SHELL);

	} else if (strcmp(name, "pwhash") == 0 || strcmp(name, "password") == 0) {
		free(ru->ru_passwd);
		ru->ru_passwd = strdup(value);

	} else if (strcmp(name, "changepw") == 0) {
		if (strcmp(value, "no") == 0) {
			UNENFORCE(ru, RES_USER_PASSWD);
		} else {
			ENFORCE(ru, RES_USER_PASSWD);
		}

	} else if (strcmp(name, "pwmin") == 0) {
		ru->ru_pwmin = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_PWMIN);

	} else if (strcmp(name, "pwmax") == 0) {
		ru->ru_pwmax = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_PWMAX);

	} else if (strcmp(name, "pwwarn") == 0) {
		ru->ru_pwwarn = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_PWWARN);

	} else if (strcmp(name, "inact") == 0) {
		ru->ru_inact = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_INACT);

	} else if (strcmp(name, "expiry") == 0 || strcmp(name, "expiration") == 0) {
		ru->ru_expire = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_EXPIRE);

	} else if (strcmp(name, "skeleton") == 0 || strcmp(name, "makehome") == 0) {
		ENFORCE(ru, RES_USER_MKHOME);
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
	assert(ru); // LCOV_EXCL_LINE

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

	assert(ru); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE
	assert(env->user_pwdb); // LCOV_EXCL_LINE
	assert(env->user_spdb); // LCOV_EXCL_LINE

	ru->different = 0;
	ru->ru_pw = pwdb_get_by_name(env->user_pwdb, ru->ru_name);
	ru->ru_sp = spdb_get_by_name(env->user_spdb, ru->ru_name);
	if (!ru->ru_pw || !ru->ru_sp) { /* new account */
		ru->different = ru->enforced;
		if (ru->ru_passwd) {
			/* always provision with password */
			DIFF(ru, RES_USER_PASSWD);
		}
		return 0;
	}

	locked = (ru->ru_sp->sp_pwdp && *(ru->ru_sp->sp_pwdp) == '!') ? 1 : 0;
	ru->different = RES_NONE;

	if (ENFORCED(ru, RES_USER_NAME) && strcmp(ru->ru_name, ru->ru_pw->pw_name) != 0) {
		DIFF(ru, RES_USER_NAME);
	}

	if (ENFORCED(ru, RES_USER_PASSWD) && strcmp(ru->ru_passwd, ru->ru_sp->sp_pwdp) != 0) {
		DIFF(ru, RES_USER_PASSWD);
	}

	if (ENFORCED(ru, RES_USER_UID) && ru->ru_uid != ru->ru_pw->pw_uid) {
		DIFF(ru, RES_USER_UID);
	}

	if (ENFORCED(ru, RES_USER_GID) && ru->ru_gid != ru->ru_pw->pw_gid) {
		DIFF(ru, RES_USER_GID);
	}

	if (ENFORCED(ru, RES_USER_GECOS) && strcmp(ru->ru_gecos, ru->ru_pw->pw_gecos) != 0) {
		DIFF(ru, RES_USER_GECOS);
	}

	if (ENFORCED(ru, RES_USER_DIR) && strcmp(ru->ru_dir, ru->ru_pw->pw_dir) != 0) {
		DIFF(ru, RES_USER_DIR);
	}

	if (ENFORCED(ru, RES_USER_SHELL) && strcmp(ru->ru_shell, ru->ru_pw->pw_shell) != 0) {
		DIFF(ru, RES_USER_SHELL);
	}

	if (ru->ru_mkhome == 1 && ENFORCED(ru, RES_USER_MKHOME)
	 && (stat(ru->ru_dir, &home) != 0 || !S_ISDIR(home.st_mode))) {
		DIFF(ru, RES_USER_MKHOME);
	}

	if (ENFORCED(ru, RES_USER_PWMIN) && ru->ru_pwmin != ru->ru_sp->sp_min) {
		DIFF(ru, RES_USER_PWMIN);
	}

	if (ENFORCED(ru, RES_USER_PWMAX) && ru->ru_pwmax != ru->ru_sp->sp_max) {
		DIFF(ru, RES_USER_PWMAX);
	}

	if (ENFORCED(ru, RES_USER_PWWARN) && ru->ru_pwwarn != ru->ru_sp->sp_warn) {
		DIFF(ru, RES_USER_PWWARN);
	}

	if (ENFORCED(ru, RES_USER_INACT) && ru->ru_inact != ru->ru_sp->sp_inact) {
		DIFF(ru, RES_USER_INACT);
	}

	if (ENFORCED(ru, RES_USER_EXPIRE) && ru->ru_expire != ru->ru_sp->sp_expire) {
		DIFF(ru, RES_USER_EXPIRE);
	}

	if (ENFORCED(ru, RES_USER_LOCK) && ru->ru_lock != locked) {
		DIFF(ru, RES_USER_LOCK);
	}

	return 0;
}

struct report* res_user_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_user *ru = (struct res_user*)(res);
	assert(ru); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE
	assert(env->user_pwdb); // LCOV_EXCL_LINE
	assert(env->user_spdb); // LCOV_EXCL_LINE

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

			/* set "password last changed" date */
			time_t now = time(NULL);
			ru->ru_sp->sp_lstchg = now / 86400;

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
			if (mkdir(ru->ru_dir, 0700) == 0
			 && chown(ru->ru_dir, ru->ru_pw->pw_uid, ru->ru_pw->pw_gid) == 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
			}
		}

		if (ru->ru_skel) {
			action = string("populate home directory from %s", ru->ru_skel);

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
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
	assert(ru); // LCOV_EXCL_LINE

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
	return res_file_clone(NULL, key);
}

void* res_file_clone(const void *res, const char *key)
{
	const struct res_file *orig = (const struct res_file*)(res);
	struct res_file *rf = xmalloc(sizeof(struct res_file));

	rf->enforced    = RES_DEFAULT(orig, enforced,  RES_NONE);
	rf->different   = RES_NONE;

	rf->rf_owner    = RES_DEFAULT_STR(orig, rf_owner, NULL);
	rf->rf_group    = RES_DEFAULT_STR(orig, rf_group, NULL);

	rf->rf_uid      = RES_DEFAULT(orig, rf_uid, 0);
	rf->rf_gid      = RES_DEFAULT(orig, rf_gid, 0);

	rf->rf_mode     = RES_DEFAULT(orig, rf_mode, 0600);

	rf->rf_lpath    = RES_DEFAULT_STR(orig, rf_lpath, NULL);
	rf->rf_rpath    = RES_DEFAULT_STR(orig, rf_rpath, NULL);
	rf->rf_template = RES_DEFAULT_STR(orig, rf_template, NULL);

	sha1_init(&(rf->rf_lsha1), NULL);
	sha1_init(&(rf->rf_rsha1), NULL);
	memset(&rf->rf_stat, 0, sizeof(struct stat));
	rf->rf_exists   = 0;


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
		free(rf->rf_rpath);
		free(rf->rf_lpath);
		free(rf->rf_template);
		free(rf->rf_owner);
		free(rf->rf_group);
		free(rf->key);
	}

	free(rf);
}

char* res_file_key(const void *res)
{
	const struct res_file *rf = (struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

	return string("file:%s", rf->key);
}

#define DUMP_UNSPEC(io,s) fprintf(io, "# %s unspecified\n", s)

int res_file_attrs(const void *res, struct hash *attrs)
{
	const struct res_file *rf = (const struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

	hash_set(attrs, "path", rf->rf_lpath);
	hash_set(attrs, "present", strdup(ENFORCED(rf, RES_FILE_ABSENT) ? "no" : "yes"));

	hash_set(attrs, "owner", ENFORCED(rf, RES_FILE_UID) ? strdup(rf->rf_owner) : NULL);
	hash_set(attrs, "group", ENFORCED(rf, RES_FILE_GID) ? strdup(rf->rf_group) : NULL);
	hash_set(attrs, "mode", ENFORCED(rf, RES_FILE_MODE) ? string("%04o", rf->rf_mode) : NULL);

	if (ENFORCED(rf, RES_FILE_SHA1)) {
		hash_set(attrs, "template", rf->rf_template ? strdup(rf->rf_template) : NULL);
		hash_set(attrs, "source",   rf->rf_rpath    ? strdup(rf->rf_rpath)    : NULL);
	} else {
		hash_set(attrs, "template", NULL);
		hash_set(attrs, "source",   NULL);
	}

	return 0;
}

int res_file_norm(void *res, struct policy *pol, struct hash *facts)
{
	struct res_file *rf = (struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

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

	/* files depend on res_dir resources between them and / */
	if (_setup_path_deps(key, rf->rf_lpath, pol) != 0) {
		free(key);
		return -1;
	}

	free(key);

	return _res_file_gen_rsha1(rf, facts);
}

int res_file_set(void *res, const char *name, const char *value)
{
	struct res_file *rf = (struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

	if (strcmp(name, "owner") == 0) {
		free(rf->rf_owner);
		rf->rf_owner = strdup(value);
		ENFORCE(rf, RES_FILE_UID);

	} else if (strcmp(name, "group") == 0) {
		free(rf->rf_group);
		rf->rf_group = strdup(value);
		ENFORCE(rf, RES_FILE_GID);

	} else if (strcmp(name, "mode") == 0) {
		/* Mask off non-permission bits */
		rf->rf_mode = strtoll(value, NULL, 0) & 07777;
		ENFORCE(rf, RES_FILE_MODE);

	} else if (strcmp(name, "source") == 0) {
		xfree(rf->rf_template);
		free(rf->rf_rpath);
		rf->rf_rpath = strdup(value);
		ENFORCE(rf, RES_FILE_SHA1);

	} else if (strcmp(name, "template") == 0) {
		xfree(rf->rf_rpath);
		free(rf->rf_template);
		rf->rf_template = strdup(value);
		ENFORCE(rf, RES_FILE_SHA1);

	} else if (strcmp(name, "path") == 0) {
		free(rf->rf_lpath);
		rf->rf_lpath = strdup(value);

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(rf, RES_FILE_ABSENT);
		} else {
			ENFORCE(rf, RES_FILE_ABSENT);
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
	assert(rf); // LCOV_EXCL_LINE

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

	assert(rf); // LCOV_EXCL_LINE
	assert(rf->rf_lpath); // LCOV_EXCL_LINE

	rf->different = 0;
	if (!rf->rf_uid && rf->rf_owner) {
		assert(env); // LCOV_EXCL_LINE
		assert(env->user_pwdb); // LCOV_EXCL_LINE
		rf->rf_uid = pwdb_lookup_uid(env->user_pwdb,  rf->rf_owner);
	}
	if (!rf->rf_gid && rf->rf_group) {
		assert(env); // LCOV_EXCL_LINE
		assert(env->group_grdb); // LCOV_EXCL_LINE
		rf->rf_gid = grdb_lookup_gid(env->group_grdb, rf->rf_group);
	}

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

	rf->different = RES_NONE;

	if (ENFORCED(rf, RES_FILE_UID) && rf->rf_uid != rf->rf_stat.st_uid) {
		DIFF(rf, RES_FILE_UID);
	}
	if (ENFORCED(rf, RES_FILE_GID) && rf->rf_gid != rf->rf_stat.st_gid) {
		DIFF(rf, RES_FILE_GID);
	}
	if (ENFORCED(rf, RES_FILE_MODE) && (rf->rf_stat.st_mode & 07777) != rf->rf_mode) {
		DIFF(rf, RES_FILE_MODE);
	}
	if (ENFORCED(rf, RES_FILE_SHA1) && memcmp(rf->rf_rsha1.raw, rf->rf_lsha1.raw, SHA1_DIGLEN) != 0) {
		DIFF(rf, RES_FILE_SHA1);
	}

	return 0;
}

struct report* res_file_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_file *rf = (struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE

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
			_mkdir_c(rf->rf_lpath);
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
		UNDIFF(rf, RES_FILE_MODE);
	}

	if (DIFFERENT(rf, RES_FILE_SHA1)) {
		assert(rf->rf_lpath); // LCOV_EXCL_LINE

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
		action = string("change permissions from %04o to %04o", rf->rf_stat.st_mode & 07777, rf->rf_mode);

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
	assert(rf); // LCOV_EXCL_LINE

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
	return res_group_clone(NULL, key);
}

void* res_group_clone(const void *res, const char *key)
{
	const struct res_group *orig = (const struct res_group*)(res);
	struct res_group *rg = xmalloc(sizeof(struct res_group));

	rg->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);
	rg->different = RES_NONE;

	rg->rg_name   = RES_DEFAULT_STR(orig, rg_name,   NULL);
	rg->rg_passwd = RES_DEFAULT_STR(orig, rg_passwd, NULL);

	rg->rg_gid    = RES_DEFAULT(orig, rg_gid, 0);

	/* FIXME: clone members of res_group */
	rg->rg_mem_add = stringlist_new(NULL);
	rg->rg_mem_rm  = stringlist_new(NULL);

	/* FIXME: clone admins of res_group */
	rg->rg_adm_add = stringlist_new(NULL);
	rg->rg_adm_rm  = stringlist_new(NULL);

	/* state variables are never cloned */
	rg->rg_grp = NULL;
	rg->rg_sg  = NULL;
	rg->rg_mem = NULL;
	rg->rg_adm = NULL;

	rg->key = NULL;
	if (key) {
		rg->key = strdup(key);
		res_group_set(rg, "name", key);
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
	assert(rg); // LCOV_EXCL_LINE

	return string("group:%s", rg->key);
}

static char* _res_group_roster_mv(struct stringlist *add, struct stringlist *rm)
{
	char *added   = NULL;
	char *removed = NULL;
	char *final   = NULL;

	if (add->num > 0) {
		added = stringlist_join(add, " ");
	}
	if (rm->num > 0) {
		removed = stringlist_join(rm, " !");
	}

	if (added && removed) {
		final = string("%s !%s", added, removed);
		xfree(added);
		xfree(removed);

	} else if (added) {
		final = added;

	} else if (removed) {
		final = string("!%s", removed);
		xfree(removed);

	} else {
		final = strdup("");

	}
	return final;
}

int res_group_attrs(const void *res, struct hash *attrs)
{
	const struct res_group *rg = (const struct res_group*)(res);
	assert(rg); // LCOV_EXCL_LINE

	hash_set(attrs, "name", strdup(rg->rg_name));
	hash_set(attrs, "gid", ENFORCED(rg, RES_GROUP_GID) ? string("%u",rg->rg_gid) : NULL);
	hash_set(attrs, "present", strdup(ENFORCED(rg, RES_GROUP_ABSENT) ? "no" : "yes"));
	hash_set(attrs, "password", ENFORCED(rg, RES_GROUP_PASSWD) ? strdup(rg->rg_passwd) : NULL);
	if (ENFORCED(rg, RES_GROUP_MEMBERS)) {
		hash_set(attrs, "members", _res_group_roster_mv(rg->rg_mem_add, rg->rg_mem_rm));
	} else {
		hash_set(attrs, "members", NULL);
	}
	if (ENFORCED(rg, RES_GROUP_ADMINS)) {
		hash_set(attrs, "admins", _res_group_roster_mv(rg->rg_adm_add, rg->rg_adm_rm));
	} else {
		hash_set(attrs, "admins", NULL);
	}
	return 0;
}

int res_group_norm(void *res, struct policy *pol, struct hash *facts) { return 0; }

int res_group_set(void *res, const char *name, const char *value)
{
	struct res_group *rg = (struct res_group*)(res);

	/* for multi-value attributes */
	struct stringlist *multi;
	size_t i;

	assert(rg); // LCOV_EXCL_LINE

	if (strcmp(name, "gid") == 0) {
		rg->rg_gid = strtoll(value, NULL, 10);
		ENFORCE(rg, RES_GROUP_GID);

	} else if (strcmp(name, "name") == 0) {
		free(rg->rg_name);
		rg->rg_name = strdup(value);
		ENFORCE(rg, RES_GROUP_NAME);

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(rg, RES_GROUP_ABSENT);
		} else {
			ENFORCE(rg, RES_GROUP_ABSENT);
		}

	} else if (strcmp(name, "member") == 0) {
		if (value[0] == '!') {
			return res_group_remove_member(rg, value+1);
		} else {
			return res_group_add_member(rg, value);
		}

	} else if (strcmp(name, "members") == 0) {
		multi = stringlist_split(value, strlen(value), " ", SPLIT_GREEDY);
		for_each_string(multi, i) {
			res_group_set(res, "member", multi->strings[i]);
		}
		stringlist_free(multi);

	} else if (strcmp(name, "admin") == 0) {
		if (value[0] == '!') {
			return res_group_remove_admin(rg, value+1);
		} else {
			return res_group_add_admin(rg, value);
		}

	} else if (strcmp(name, "admins") == 0) {
		multi = stringlist_split(value, strlen(value), " ", SPLIT_GREEDY);
		for_each_string(multi, i) {
			res_group_set(res, "admin", multi->strings[i]);
		}
		stringlist_free(multi);

	} else if (strcmp(name, "pwhash") == 0 || strcmp(name, "password") == 0) {
		free(rg->rg_passwd);
		rg->rg_passwd = strdup(value);

	} else if (strcmp(name, "changepw") == 0) {
		if (strcmp(value, "no") == 0) {
			UNENFORCE(rg, RES_GROUP_PASSWD);
		} else {
			ENFORCE(rg, RES_GROUP_PASSWD);
		}

	} else {
		return -1;
	}

	return 0;
}

int res_group_match(const void *res, const char *name, const char *value)
{
	const struct res_group *rg = (struct res_group*)(res);
	assert(rg); // LCOV_EXCL_LINE

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
	assert(rg); // LCOV_EXCL_LINE

	if (enforce) {
		ENFORCE(rg, RES_GROUP_MEMBERS);
	} else {
		UNENFORCE(rg, RES_GROUP_MEMBERS);
	}
	return 0;
}

/**
  Add $user to the list of members.

  On success, returns 0.  On failure, returns non-zero.
 */
int res_group_add_member(struct res_group *rg, const char *user)
{
	assert(rg); // LCOV_EXCL_LINE
	assert(user); // LCOV_EXCL_LINE

	res_group_enforce_members(rg, 1);
	/* add to rg_mem_add, remove from rg_mem_rm */
	return _group_update(rg->rg_mem_add, rg->rg_mem_rm, user);
}

/**
  Remove $user from the list of members.

  On success, returns 0.  On failure, returns non-zero.
 */
int res_group_remove_member(struct res_group *rg, const char *user)
{
	assert(rg); // LCOV_EXCL_LINE
	assert(user); // LCOV_EXCL_LINE

	res_group_enforce_members(rg, 1);
	/* add to rg_mem_rm, remove from rg_mem_add */
	return _group_update(rg->rg_mem_rm, rg->rg_mem_add, user);
}

int res_group_enforce_admins(struct res_group *rg, int enforce)
{
	assert(rg); // LCOV_EXCL_LINE

	if (enforce) {
		ENFORCE(rg, RES_GROUP_ADMINS);
	} else {
		UNENFORCE(rg, RES_GROUP_ADMINS);
	}
	return 0;
}

/**
  Add $user to the list of admins.

  On success, returns 0.  On failure, returns non-zero.
 */
int res_group_add_admin(struct res_group *rg, const char *user)
{
	assert(rg); // LCOV_EXCL_LINE
	assert(user); // LCOV_EXCL_LINE

	res_group_enforce_admins(rg, 1);
	/* add to rg_adm_add, remove from rg_adm_rm */
	return _group_update(rg->rg_adm_add, rg->rg_adm_rm, user);
}

/**
  Remove $user from the list of admins.

  On success, returns 0.  On failure, returns non-zero.
 */
int res_group_remove_admin(struct res_group *rg, const char *user)
{
	assert(rg); // LCOV_EXCL_LINE
	assert(user); // LCOV_EXCL_LINE

	res_group_enforce_admins(rg, 1);
	/* add to rg_adm_rm, remove from rg_adm_add */
	return _group_update(rg->rg_adm_rm, rg->rg_adm_add, user);
}

int res_group_stat(void *res, const struct resource_env *env)
{
	struct res_group *rg = (struct res_group*)(res);
	struct stringlist *list;

	assert(rg); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE
	assert(env->group_grdb); // LCOV_EXCL_LINE
	assert(env->group_sgdb); // LCOV_EXCL_LINE

	rg->different = 0;
	rg->rg_grp = grdb_get_by_name(env->group_grdb, rg->rg_name);
	rg->rg_sg = sgdb_get_by_name(env->group_sgdb, rg->rg_name);
	if (!rg->rg_grp || !rg->rg_sg) { /* new group */
		rg->different = rg->enforced;

		rg->rg_mem = stringlist_new(NULL);
		stringlist_add_all(rg->rg_mem, rg->rg_mem_add);

		rg->rg_adm = stringlist_new(NULL);
		stringlist_add_all(rg->rg_adm, rg->rg_adm_add);

		if (rg->rg_passwd) {
			/* always provision with password */
			DIFF(rg, RES_GROUP_PASSWD);
		}

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

	rg->different = RES_NONE;

	if (ENFORCED(rg, RES_GROUP_NAME) && strcmp(rg->rg_name, rg->rg_grp->gr_name) != 0) {
		DIFF(rg, RES_GROUP_NAME);
	}

	if (ENFORCED(rg, RES_GROUP_PASSWD) && strcmp(rg->rg_passwd, rg->rg_sg->sg_passwd) != 0) {
		DIFF(rg, RES_GROUP_PASSWD);
	}

	if (ENFORCED(rg, RES_GROUP_GID) && rg->rg_gid != rg->rg_grp->gr_gid) {
		DIFF(rg, RES_GROUP_GID);
	}

	if (ENFORCED(rg, RES_GROUP_MEMBERS)) {
		/* use list as a stringlist of gr_mem */
		list = stringlist_new(rg->rg_grp->gr_mem);
		if (stringlist_diff(list, rg->rg_mem) == 0) {
			DIFF(rg, RES_GROUP_MEMBERS);
		}
		stringlist_free(list);
	}

	if (ENFORCED(rg, RES_GROUP_ADMINS)) {
		/* use list as a stringlist of sg_adm */
		list = stringlist_new(rg->rg_sg->sg_adm);
		if (stringlist_diff(list, rg->rg_adm) == 0) {
			DIFF(rg, RES_GROUP_ADMINS);
		}
		stringlist_free(list);
	}

	return 0;
}

struct report* res_group_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_group *rg = (struct res_group*)(res);
	assert(rg); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE
	assert(env->group_grdb); // LCOV_EXCL_LINE
	assert(env->group_sgdb); // LCOV_EXCL_LINE

	struct report *report;
	char *action;

	int new_group = 0;
	struct stringlist *orig;
	struct stringlist *to_add;
	struct stringlist *to_remove;

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
	assert(rg); // LCOV_EXCL_LINE

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
	rg->rg_mem_add = stringlist_split(mem_add, strlen(mem_add), ".", 0);

	stringlist_free(rg->rg_mem_rm);
	rg->rg_mem_rm  = stringlist_split(mem_rm,  strlen(mem_rm),  ".", 0);

	stringlist_free(rg->rg_adm_add);
	rg->rg_adm_add = stringlist_split(adm_add, strlen(adm_add), ".", 0);

	stringlist_free(rg->rg_adm_rm);
	rg->rg_adm_rm  = stringlist_split(adm_rm,  strlen(adm_rm),  ".", 0);

	free(mem_add); free(mem_rm);
	free(adm_add); free(adm_rm);

	return rg;
}
#undef PACK_FORMAT

int res_group_notify(void *res, const struct resource *dep) { return 0; }

/*****************************************************************/

void* res_package_new(const char *key)
{
	return res_package_clone(NULL, key);
}

void* res_package_clone(const void *res, const char *key)
{
	const struct res_package *orig = (const struct res_package*)(res);
	struct res_package *rp = xmalloc(sizeof(struct res_package));

	rp->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);
	rp->different = RES_NONE;

	rp->version   = RES_DEFAULT_STR(orig, version, NULL);

	/* state variables are never cloned */
	rp->installed = NULL;

	rp->key = NULL;
	if (key) {
		rp->key = strdup(key);
		res_package_set(rp, "name", key);
	}

	return rp;
}

void res_package_free(void *res)
{
	struct res_package *rp = (struct res_package*)(res);
	if (rp) {
		free(rp->name);
		free(rp->version);

		free(rp->key);
	}

	free(rp);
}

char* res_package_key(const void *res)
{
	const struct res_package *rp = (struct res_package*)(res);
	assert(rp); // LCOV_EXCL_LINE

	return string("package:%s", rp->key);
}

int res_package_attrs(const void *res, struct hash *attrs)
{
	const struct res_package *rp = (const struct res_package*)(res);
	assert(rp); // LCOV_EXCL_LINE

	hash_set(attrs, "name", xstrdup(rp->name));
	hash_set(attrs, "version", xstrdup(rp->version));
	hash_set(attrs, "installed", strdup(ENFORCED(rp, RES_PACKAGE_ABSENT) ? "no" : "yes"));
	return 0;
}

int res_package_norm(void *res, struct policy *pol, struct hash *facts) { return 0; }

int res_package_set(void *res, const char *name, const char *value)
{
	struct res_package *rp = (struct res_package*)(res);
	assert(rp); // LCOV_EXCL_LINE

	if (strcmp(name, "name") == 0) {
		free(rp->name);
		rp->name = strdup(value);

	} else if (strcmp(name, "version") == 0) {
		free(rp->version);
		rp->version = strdup(value);

	} else if (strcmp(name, "installed") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(rp, RES_PACKAGE_ABSENT);
		} else {
			ENFORCE(rp, RES_PACKAGE_ABSENT);
		}

	} else {
		return -1;
	}

	return 0;
}

int res_package_match(const void *res, const char *name, const char *value)
{
	const struct res_package *rp = (const struct res_package*)(res);
	assert(rp); // LCOV_EXCL_LINE

	char *test_value;
	int rc;

	if (strcmp(name, "name") == 0) {
		test_value = string("%s", rp->name);
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

	assert(rp); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE
	assert(env->package_manager); // LCOV_EXCL_LINE

	free(rp->installed);
	rp->installed = package_version(env->package_manager, rp->name);

	if (xstrcmp(rp->version, "latest") == 0) {
		rp->version = package_latest(env->package_manager, rp->name);
	}

	return 0;
}

struct report* res_package_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_package *rp = (struct res_package*)(res);

	assert(rp); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE
	assert(env->package_manager); // LCOV_EXCL_LINE

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
	assert(rp); // LCOV_EXCL_LINE

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

/*****************************************************************/

void* res_service_new(const char *key)
{
	return res_service_clone(NULL, key);
}

void* res_service_clone(const void *res, const char *key)
{
	const struct res_service *orig = (const struct res_service*)(res);
	struct res_service *rs = xmalloc(sizeof(struct res_service));

	rs->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);
	rs->different = RES_NONE;

	/* state variables are never cloned */
	rs->notified = 0;
	rs->running = 0;
	rs->enabled = 0;

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
		free(rs->service);

		free(rs->key);
	}

	free(rs);
}

char* res_service_key(const void *res)
{
	const struct res_service *rs = (struct res_service*)(res);
	assert(rs); // LCOV_EXCL_LINE

	return string("service:%s", rs->key);
}

int res_service_attrs(const void *res, struct hash *attrs)
{
	const struct res_service *rs = (const struct res_service*)(res);
	assert(rs); // LCOV_EXCL_LINE

	hash_set(attrs, "name", xstrdup(rs->service));
	hash_set(attrs, "running", strdup(ENFORCED(rs, RES_SERVICE_RUNNING) ? "yes" : "no"));
	hash_set(attrs, "enabled", strdup(ENFORCED(rs, RES_SERVICE_ENABLED) ? "yes" : "no"));
	return 0;
}

int res_service_norm(void *res, struct policy *pol, struct hash *facts) { return 0; }

int res_service_set(void *res, const char *name, const char *value)
{
	struct res_service *rs = (struct res_service*)(res);
	assert(rs); // LCOV_EXCL_LINE

	if (strcmp(name, "name") == 0 || strcmp(name, "service") == 0) {
		free(rs->service);
		rs->service = strdup(value);

	} else if (strcmp(name, "running") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(rs, RES_SERVICE_STOPPED);
			ENFORCE(rs, RES_SERVICE_RUNNING);
		} else {
			UNENFORCE(rs, RES_SERVICE_RUNNING);
			ENFORCE(rs, RES_SERVICE_STOPPED);
		}

	} else if (strcmp(name, "stopped") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(rs, RES_SERVICE_RUNNING);
			ENFORCE(rs, RES_SERVICE_STOPPED);
		} else {
			UNENFORCE(rs, RES_SERVICE_STOPPED);
			ENFORCE(rs, RES_SERVICE_RUNNING);
		}

	} else if (strcmp(name, "enabled") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(rs, RES_SERVICE_DISABLED);
			ENFORCE(rs, RES_SERVICE_ENABLED);
		} else {
			UNENFORCE(rs, RES_SERVICE_ENABLED);
			ENFORCE(rs, RES_SERVICE_DISABLED);
		}

	} else if (strcmp(name, "disabled") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(rs, RES_SERVICE_ENABLED);
			ENFORCE(rs, RES_SERVICE_DISABLED);
		} else {
			UNENFORCE(rs, RES_SERVICE_DISABLED);
			ENFORCE(rs, RES_SERVICE_ENABLED);
		}

	} else {
		return -1;
	}

	return 0;
}

int res_service_match(const void *res, const char *name, const char *value)
{
	const struct res_service *rs = (const struct res_service*)(res);
	assert(rs); // LCOV_EXCL_LINE

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

	assert(rs); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE
	assert(env->service_manager); // LCOV_EXCL_LINE

	rs->enabled = (service_enabled(env->service_manager, rs->service) == 0 ? 1 : 0);
	rs->running = (service_running(env->service_manager, rs->service) == 0 ? 1 : 0);

	return 0;
}

struct report* res_service_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_service *rs = (struct res_service*)(res);

	assert(rs); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE
	assert(env->service_manager); // LCOV_EXCL_LINE

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
	assert(rs); // LCOV_EXCL_LINE

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
	assert(rs); // LCOV_EXCL_LINE

	rs->notified = 1;

	return 0;
}

/*****************************************************************/

void* res_host_new(const char *key)
{
	return res_host_clone(NULL, key);
}

void* res_host_clone(const void *res, const char *key)
{
	const struct res_host *orig = (const struct res_host*)(res);
	struct res_host *rh = xmalloc(sizeof(struct res_host));

	rh->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);
	rh->different = RES_NONE;

	/* FIXME: clone host aliases */
	rh->aliases = stringlist_new(NULL);

	rh->ip = RES_DEFAULT_STR(orig, ip, NULL);

	/* state variables are never cloned */
	rh->aug_root = NULL;

	rh->key = NULL;
	if (key) {
		rh->key = strdup(key);
		res_host_set(rh, "hostname", key);
	}

	return rh;
}

void res_host_free(void *res)
{
	struct res_host *rh = (struct res_host*)(res);
	if (rh) {
		free(rh->hostname);
		stringlist_free(rh->aliases);

		free(rh->key);
	}

	free(rh);
}

char* res_host_key(const void *res)
{
	const struct res_host *rh = (struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE

	return string("host:%s", rh->key);
}

int res_host_attrs(const void *res, struct hash *attrs)
{
	const struct res_host *rh = (const struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE

	hash_set(attrs, "hostname", xstrdup(rh->hostname));
	hash_set(attrs, "present",  strdup(ENFORCED(rh, RES_HOST_ABSENT) ? "no" : "yes"));
	hash_set(attrs, "ip", xstrdup(rh->ip));
	if (ENFORCED(rh, RES_HOST_ALIASES)) {
		hash_set(attrs, "aliases", stringlist_join(rh->aliases, " "));
	} else {
		hash_set(attrs, "aliases", NULL);
	}
	return 0;
}

int res_host_norm(void *res, struct policy *pol, struct hash *facts) { return 0; }

int res_host_set(void *res, const char *name, const char *value)
{
	struct res_host *rh = (struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE
	struct stringlist *alias_tmp;

	if (streq(name, "hostname")) {
		free(rh->hostname);
		rh->hostname = strdup(value);

	} else if (streq(name, "ip") || streq(name, "address")) {
		free(rh->ip);
		rh->ip = strdup(value);

	} else if (streq(name, "aliases") || streq(name, "alias")) {
		alias_tmp = stringlist_split(value, strlen(value), " ", SPLIT_GREEDY);
		if (stringlist_add_all(rh->aliases, alias_tmp) != 0) {
			stringlist_free(alias_tmp);
			return -1;
		}
		stringlist_free(alias_tmp);

		ENFORCE(rh, RES_HOST_ALIASES);

	} else if (streq(name, "present")) {
		if (streq(value, "no")) {
			ENFORCE(rh, RES_HOST_ABSENT);
		} else {
			UNENFORCE(rh, RES_HOST_ABSENT);
		}

	} else {
		return -1;
	}

	return 0;
}

int res_host_match(const void *res, const char *name, const char *value)
{
	const struct res_host *rh = (const struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE

	char *test_value;
	int rc;

	if (strcmp(name, "hostname") == 0) {
		test_value = string("%s", rh->hostname);
	} else if (strcmp(name, "ip") == 0 || strcmp(name, "address") == 0) {
		test_value = string("%s", rh->ip);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_host_stat(void *res, const struct resource_env *env)
{
	struct res_host *rh = (struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE
	assert(env->aug_context); // LCOV_EXCL_LINE

	char *tmp, **results;
	const char *value;
	int rc, i;
	struct stringlist *real_aliases = NULL;

	rh->different = 0;
	xfree(rh->aug_root);
	DEBUG("res_host: stat %p // %s", rh, rh->hostname);

	rc = aug_match(env->aug_context, "/files/etc/hosts/*", &results);
	DEBUG("res_host: found %u entries under /files/etc/hosts", rc);
	for (i = 0; i < rc; i++) {
		tmp = string("%s/ipaddr", results[i]);
		DEBUG("res_host: checking %s", tmp);
		aug_get(env->aug_context, tmp, &value);
		free(tmp);

		DEBUG("res_host: eval found ip(%s) against res_host ip(%s)", value, rh->ip);
		if (value && strcmp(value, rh->ip) == 0) { // ip matched
			tmp = string("%s/canonical", results[i]);
			DEBUG("res_host: checking %s", tmp);
			aug_get(env->aug_context, tmp, &value);
			free(tmp);

			DEBUG("res_host: eval found hostname(%s) against res_host hostname(%s)", value, rh->hostname);
			if (value && strcmp(value, rh->hostname) == 0) {
				rh->aug_root = strdup(results[i]);
				DEBUG("res_host: found match at %s", rh->aug_root);
				break;
			}
		}
	}
	free(results);

	if (ENFORCED(rh, RES_HOST_ALIASES)) {
		if (rh->aug_root) {
			tmp = string("%s/alias", rh->aug_root);
			real_aliases = augcw_getm(env->aug_context, tmp);
			free(tmp);

			if (!real_aliases ||
			    stringlist_diff(rh->aliases, real_aliases) == 0) {

				DIFF(rh, RES_HOST_ALIASES);
			}
			stringlist_free(real_aliases);

		} else {
			DIFF(rh, RES_HOST_ALIASES);
		}
	}

	return 0;
}

struct report* res_host_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_host *rh = (struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE

	char *tmp1, *tmp2;
	int i;

	struct report *report;
	char *action;

	int just_created = 0;

	report = report_new("Host Entry", rh->hostname);

	if (ENFORCED(rh, RES_HOST_ABSENT)) {
		if (rh->aug_root) {
			action = string("remove host entry");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
				return report;
			}

			if (aug_rm(env->aug_context, rh->aug_root) > 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
			}

			return report;
		}

	} else {
		if (!rh->aug_root) {
			action = string("create host entry");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
			} else {
				rh->aug_root = string("/files/etc/hosts/0%u", rh);
				tmp1 = string("%s/ipaddr",    rh->aug_root);
				tmp2 = string("%s/canonical", rh->aug_root);

				DEBUG("res_host: Setting %s to %s", tmp1, rh->ip);
				DEBUG("res_host: Setting %s to %s", tmp2, rh->hostname);

				if (aug_set(env->aug_context, tmp1, rh->ip) < 0
				 || aug_set(env->aug_context, tmp2, rh->hostname) < 0) {

					report_action(report, action, ACTION_FAILED);
					free(rh->aug_root);
					rh->aug_root = NULL;
				} else {
					report_action(report, action, ACTION_SUCCEEDED);
					just_created = 1;
				}

				free(tmp1);
				free(tmp2);
			}
		}
	}

	if (DIFFERENT(rh, RES_HOST_ALIASES) && rh->aliases) {
		action = string("setting host aliases");

		if (dryrun && !just_created) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			tmp1 = string("%s/alias", rh->aug_root);
			DEBUG("res_host: removing %s", tmp1);
			if (aug_rm(env->aug_context, tmp1) < 0) {
				report_action(report, action, ACTION_FAILED);
				action = NULL; // prevent future report for this action
			}
			free(tmp1);

			for (i = 0; i < rh->aliases->num; i++) {
				tmp1 = string("%s/alias[%u]", rh->aug_root, i+1);
				DEBUG("res_host: set %s to %s", tmp1, rh->aliases->strings[i]);
				if (aug_set(env->aug_context, tmp1, rh->aliases->strings[i]) < 0
				 && action) {
					report_action(report, action, ACTION_FAILED);
					action = NULL; // prevent future report
				}
				free(tmp1);
			}

			if (action) {
				report_action(report, action, ACTION_SUCCEEDED);
			}
		}
	}

	return report;
}

#define PACK_FORMAT "aLaaa"
char* res_host_pack(const void *res)
{
	const struct res_host *rh = (const struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE

	char *joined;
	char *p;

	joined = stringlist_join(rh->aliases, " ");

	p = pack("res_host::", PACK_FORMAT,
	         rh->key, rh->enforced,
	         rh->hostname, rh->ip, joined);

	free(joined);
	return p;
}

void* res_host_unpack(const char *packed)
{
	struct res_host *rh = res_host_new(NULL);
	char *joined = NULL;

	if (unpack(packed, "res_host::", PACK_FORMAT,
		&rh->key, &rh->enforced,
		&rh->hostname, &rh->ip, &joined) != 0) {

		res_host_free(rh);
		free(joined);
		return NULL;
	}

	stringlist_free(rh->aliases);
	rh->aliases = stringlist_split(joined, strlen(joined), " ", SPLIT_GREEDY);

	return rh;
}
#undef PACK_FORMAT

int res_host_notify(void *res, const struct resource *dep) { return 0; }

/*****************************************************************/

void* res_sysctl_new(const char *key)
{
	return res_sysctl_clone(NULL, key);
}

void* res_sysctl_clone(const void *res, const char *key)
{
	const struct res_sysctl *orig = (const struct res_sysctl*)(res);
	struct res_sysctl *rs = xmalloc(sizeof(struct res_sysctl));

	rs->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);
	rs->different = RES_NONE;

	/* persist sysctl changes, by default */
	rs->persist = RES_DEFAULT(orig, persist, 1);

	rs->param = RES_DEFAULT_STR(orig, param, NULL);
	rs->value = RES_DEFAULT_STR(orig, value, NULL);

	rs->key = NULL;
	if (key) {
		rs->key = strdup(key);
		res_sysctl_set(rs, "param", key);
	}

	return rs;
}

void res_sysctl_free(void *res)
{
	struct res_sysctl *rs = (struct res_sysctl*)(res);
	if (rs) {
		free(rs->param);
		free(rs->value);

		free(rs->key);
	}

	free(rs);
}

char* res_sysctl_key(const void *res)
{
	const struct res_sysctl *rs = (struct res_sysctl*)(res);
	assert(rs); // LCOV_EXCL_LINE

	return string("sysctl:%s", rs->key);
}

int res_sysctl_attrs(const void *res, struct hash *attrs)
{
	const struct res_sysctl *rs = (const struct res_sysctl*)(res);
	assert(rs); // LCOV_EXCL_LINE

	hash_set(attrs, "param", xstrdup(rs->param));
	hash_set(attrs, "value", ENFORCED(rs, RES_SYSCTL_VALUE) ? strdup(rs->value) : NULL);
	hash_set(attrs, "persist", strdup(ENFORCED(rs, RES_SYSCTL_PERSIST) ? "yes" : "no"));
	return 0;
}

int res_sysctl_norm(void *res, struct policy *pol, struct hash *facts) { return 0; }

int res_sysctl_set(void *res, const char *name, const char *value)
{
	struct res_sysctl *rs = (struct res_sysctl*)(res);
	assert(rs); // LCOV_EXCL_LINE

	if (strcmp(name, "param") == 0) {
		free(rs->param);
		rs->param = strdup(value);

	} else if (strcmp(name, "value") == 0) {
		free(rs->value);
		rs->value = strdup(value);
		ENFORCE(rs, RES_SYSCTL_VALUE);

	} else if (strcmp(name, "persist") == 0) {
		rs->persist = strcmp(value, "no");
		if (rs->persist) {
			ENFORCE(rs, RES_SYSCTL_PERSIST);
		} else {
			UNENFORCE(rs, RES_SYSCTL_PERSIST);
		}

	} else {
		return -1;
	}

	return 0;
}

int res_sysctl_match(const void *res, const char *name, const char *value)
{
	const struct res_sysctl *rs = (const struct res_sysctl*)(res);
	assert(rs); // LCOV_EXCL_LINE

	char *test_value;
	int rc;

	if (strcmp(name, "param") == 0) {
		test_value = string("%s", rs->param);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_sysctl_stat(void *res, const struct resource_env *env)
{
	struct res_sysctl *rs = (struct res_sysctl*)(res);
	assert(rs); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE

	char *tmp;
	const char *aug_value;

	rs->different = 0;
	if (ENFORCED(rs, RES_SYSCTL_VALUE)) {
		if (_sysctl_read(rs->param, &tmp) != 0) {
			WARNING("res_sysctl: failed to get live value of %s", rs->param);
			return -1;
		}

		if (!streq(rs->value, tmp)) {
			DEBUG("'%s' != '%s'", rs->value, tmp);
			DIFF(rs, RES_SYSCTL_VALUE);
		}
		xfree(tmp);

		if (ENFORCED(rs, RES_SYSCTL_PERSIST)) {
			tmp = string("/files/etc/sysctl.conf/%s", rs->param);
			if (aug_get(env->aug_context, tmp, &aug_value) != 1
			 || strcmp(aug_value, rs->value) != 0) {

				DIFF(rs, RES_SYSCTL_PERSIST);
			}
			xfree(tmp);
		}
	}

	return 0;
}

struct report* res_sysctl_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_sysctl *rs = (struct res_sysctl*)(res);
	assert(rs); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE

	char *aug_path;

	struct report *report;
	char *action;

	report = report_new("Sysctl", rs->param);

	if (DIFFERENT(rs, RES_SYSCTL_VALUE)) {
		action = string("set kernel param to '%s' via /proc/sys", rs->value);
		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			if (_sysctl_write(rs->param, rs->value) > 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
			}
		}
	}

	if (DIFFERENT(rs, RES_SYSCTL_PERSIST)) {
		action = string("save setting in /etc/sysctl.conf");
		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else {
			aug_path = string("/files/etc/sysctl.conf/%s", rs->param);
			if (aug_set(env->aug_context, aug_path, rs->value) < 0) {
				report_action(report, action, ACTION_FAILED);
			} else {
				report_action(report, action, ACTION_SUCCEEDED);
			}
			free(aug_path);
		}
	}

	return report;
}

#define PACK_FORMAT "aLaaS"
char* res_sysctl_pack(const void *res)
{
	const struct res_sysctl *rs = (const struct res_sysctl*)(res);
	assert(rs); // LCOV_EXCL_LINE

	return pack("res_sysctl::", PACK_FORMAT,
		rs->key, rs->enforced,
		rs->param, rs->value, rs->persist ? 1 : 0);
}

void* res_sysctl_unpack(const char *packed)
{
	struct res_sysctl *rs = res_sysctl_new(NULL);

	if (unpack(packed, "res_sysctl::", PACK_FORMAT,
		&rs->key, &rs->enforced,
		&rs->param, &rs->value, &rs->persist) != 0) {

		res_sysctl_free(rs);
		return NULL;
	}

	return rs;
}
#undef PACK_FORMAT

int res_sysctl_notify(void* res, const struct resource *dep) { return 0; }

/*****************************************************************/

void* res_dir_new(const char *key)
{
	return res_dir_clone(NULL, key);
}

void* res_dir_clone(const void *res, const char *key)
{
	const struct res_dir *orig = (const struct res_dir*)(res);
	struct res_dir *rd = xmalloc(sizeof(struct res_dir));

	rd->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);
	rd->different = RES_NONE;

	rd->owner = RES_DEFAULT_STR(orig, owner, NULL);
	rd->group = RES_DEFAULT_STR(orig, group, NULL);

	rd->uid   = RES_DEFAULT(orig, uid, 0);
	rd->gid   = RES_DEFAULT(orig, gid, 0);
	rd->mode  = RES_DEFAULT(orig, mode, 0700);

	/* state variables are never cloned */
	memset(&rd->stat, 0, sizeof(struct stat));
	rd->exists = 0;

	rd->key = NULL;
	if (key) {
		rd->key = strdup(key);
		res_dir_set(rd, "path", key);
	}

	return rd;
}

void res_dir_free(void *res)
{
	struct res_dir *rd = (struct res_dir*)(res);
	if (rd) {
		free(rd->path);
		free(rd->owner);
		free(rd->group);
		free(rd->key);
	}
	free(rd);
}

char *res_dir_key(const void *res)
{
	const struct res_dir *rd = (const struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE

	return string("dir:%s", rd->key);
}

int res_dir_attrs(const void *res, struct hash *attrs)
{
	const struct res_dir *rd = (const struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE

	hash_set(attrs, "path", xstrdup(rd->path));
	hash_set(attrs, "owner", ENFORCED(rd, RES_DIR_UID) ? strdup(rd->owner) : NULL);
	hash_set(attrs, "group", ENFORCED(rd, RES_DIR_GID) ? strdup(rd->group) : NULL);
	hash_set(attrs, "mode", ENFORCED(rd, RES_DIR_MODE) ? string("%04o", rd->mode) : NULL);
	hash_set(attrs, "present", strdup(ENFORCED(rd, RES_DIR_ABSENT) ? "no" : "yes"));
	return 0;
}

int res_dir_norm(void *res, struct policy *pol, struct hash *facts)
{
	struct res_dir *rd = (struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE

	struct dependency *dep;
	struct resource *other;
	char *key = res_dir_key(rd);

	/* directories depend on their owner and group */
	if (ENFORCED(rd, RES_DIR_UID)) {
		other = policy_find_resource(pol, RES_USER, "username", rd->owner);
		if (other) {
			dep = dependency_new(key, other->key);
			if (policy_add_dependency(pol, dep) != 0) {
				dependency_free(dep);
				free(key);
				return -1;
			}
		}
	}

	if (ENFORCED(rd, RES_DIR_GID)) {
		other = policy_find_resource(pol, RES_GROUP, "name", rd->group);
		if (other) {
			dep = dependency_new(key, other->key);
			if (policy_add_dependency(pol, dep) != 0) {
				dependency_free(dep);
				free(key);
				return -1;
			}
		}
	}

	/* directories depend on other res_dir resources between them and / */
	if (_setup_path_deps(key, rd->path, pol) != 0) {
		free(key);
		return -1;
	}

	free(key);
	return 0;
}

int res_dir_set(void *res, const char *name, const char *value)
{
	struct res_dir *rd = (struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE

	if (strcmp(name, "owner") == 0) {
		free(rd->owner);
		rd->owner = strdup(value);
		ENFORCE(rd, RES_DIR_UID);

	} else if (strcmp(name, "group") == 0) {
		free(rd->group);
		rd->group = strdup(value);
		ENFORCE(rd, RES_DIR_GID);

	} else if (strcmp(name, "mode") == 0) {
		/* Mask off non-permission bits */
		rd->mode = strtoll(value, NULL, 0) & 07777;
		ENFORCE(rd, RES_DIR_MODE);

	} else if (strcmp(name, "path") == 0) {
		free(rd->path);
		rd->path = strdup(value);

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(rd, RES_DIR_ABSENT);
		} else {
			ENFORCE(rd, RES_DIR_ABSENT);
		}

	} else {
		return -1;

	}

	return 0;
}

int res_dir_match(const void *res, const char *name, const char *value)
{
	const struct res_dir *rd = (struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE

	char *test_value;
	int rc;

	if (strcmp(name, "path") == 0) {
		test_value = string("%s", rd->path);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_dir_stat(void *res, const struct resource_env *env)
{
	struct res_dir *rd = (struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE
	assert(rd->path); // LCOV_EXCL_LINE

	rd->different = 0;
	if (!rd->uid && rd->owner) {
		assert(env);            // LCOV_EXCL_LINE
		assert(env->user_pwdb); // LCOV_EXCL_LINE
		rd->uid = pwdb_lookup_uid(env->user_pwdb,  rd->owner);
	}
	if (!rd->gid && rd->group) {
		assert(env);             // LCOV_EXCL_LINE
		assert(env->group_grdb); // LCOV_EXCL_LINE
		rd->gid = grdb_lookup_gid(env->group_grdb, rd->group);
	}

	if (stat(rd->path, &rd->stat) == -1) { /* new directory */
		rd->different = rd->enforced;
		rd->exists = 0;
		return 0;
	}
	rd->exists = 1;

	rd->different = RES_NONE;

	if (ENFORCED(rd, RES_DIR_UID) && rd->uid != rd->stat.st_uid) {
		DIFF(rd, RES_DIR_UID);
	}
	if (ENFORCED(rd, RES_DIR_GID) && rd->gid != rd->stat.st_gid) {
		DIFF(rd, RES_DIR_GID);
	}
	if (ENFORCED(rd, RES_DIR_MODE) && (rd->stat.st_mode & 07777) != rd->mode) {
		DIFF(rd, RES_DIR_MODE);
	}

	return 0;
}

struct report* res_dir_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_dir *rd = (struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE

	struct report *report = report_new("Directory", rd->path);
	char *action;
	int new_dir = 0;

	/* Remove the directory */
	/* FIXME: do we remove the entire directory? */
	if (ENFORCED(rd, RES_DIR_ABSENT)) {
		if (rd->exists == 1) {
			action = string("remove directory");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
			} else if (rmdir(rd->path) == 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
			}
		}

		return report;
	}

	if (!rd->exists) {
		new_dir = 1;
		action = string("create directory");

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (_mkdir_p(rd->path) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
			return report;
		}

		rd->different = rd->enforced;
	}

	if (DIFFERENT(rd, RES_DIR_UID)) {
		if (new_dir) {
			action = string("set owner to %s(%u)", rd->owner, rd->uid);
		} else {
			action = string("change owner from %u to %s(%u)", rd->stat.st_uid, rd->owner, rd->uid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (chown(rd->path, rd->uid, -1) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	if (DIFFERENT(rd, RES_DIR_GID)) {
		if (new_dir) {
			action = string("set group to %s(%u)", rd->group, rd->gid);
		} else {
			action = string("change group from %u to %s(%u)", rd->stat.st_gid, rd->group, rd->gid);
		}

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (chown(rd->path, -1, rd->gid) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	if (DIFFERENT(rd, RES_DIR_MODE)) {
		action = string("change permissions from %04o to %04o", rd->stat.st_mode & 07777, rd->mode);

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (chmod(rd->path, rd->mode) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	return report;
}

#define PACK_FORMAT "aLaaaL"
char *res_dir_pack(const void *res)
{
	const struct res_dir *rd = (const struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE

	return pack("res_dir::", PACK_FORMAT,
		rd->key, rd->enforced,
		rd->path, rd->owner, rd->group, rd->mode);
}

void* res_dir_unpack(const char *packed)
{
	struct res_dir *rd = res_dir_new(NULL);

	if (unpack(packed, "res_dir::", PACK_FORMAT,
		&rd->key, &rd->enforced,
		&rd->path, &rd->owner, &rd->group, &rd->mode) != 0) {

		res_dir_free(rd);
		return NULL;
	}

	return rd;
}
#undef PACK_FORMAT

int res_dir_notify(void *res, const struct resource *dep) { return 0; }

/********************************************************************/

void* res_exec_new(const char *key)
{
	return res_exec_clone(NULL, key);
}

void* res_exec_clone(const void *res, const char *key)
{
	const struct res_exec *orig = (const struct res_exec*)(res);
	struct res_exec *re = xmalloc(sizeof(struct res_exec));

	re->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);
	re->different = RES_NONE;

	re->user  = RES_DEFAULT_STR(orig, user,  NULL);
	re->group = RES_DEFAULT_STR(orig, group, NULL);

	re->uid   = RES_DEFAULT(orig, uid, 0);
	re->gid   = RES_DEFAULT(orig, gid, 0);

	re->command = RES_DEFAULT_STR(orig, command, NULL);
	re->test    = RES_DEFAULT_STR(orig, test,    NULL);

	re->key = NULL;
	if (key) {
		re->key = strdup(key);
		res_exec_set(re, "command", key);
	}

	return re;
}

void res_exec_free(void *res)
{
	struct res_exec *re = (struct res_exec*)(res);
	if (re) {
		xfree(re->command);
		xfree(re->test);
		xfree(re->user);
		xfree(re->group);
		xfree(re->key);
	}
	xfree(re);
}

char *res_exec_key(const void *res)
{
	const struct res_exec *re = (const struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE

	return string("exec:%s", re->key);
}

int res_exec_attrs(const void *res, struct hash *attrs)
{
	const struct res_exec *re = (const struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE

	hash_set(attrs, "command", xstrdup(re->command));
	hash_set(attrs, "test",    xstrdup(re->test));
	hash_set(attrs, "user", ENFORCED(re, RES_EXEC_UID) ? strdup(re->user) : NULL);
	hash_set(attrs, "group", ENFORCED(re, RES_EXEC_GID) ? strdup(re->group) : NULL);
	hash_set(attrs, "ondemand", strdup(ENFORCED(re, RES_EXEC_ONDEMAND) ? "yes" : "no"));
	return 0;
}

int res_exec_norm(void *res, struct policy *pol, struct hash *facts)
{
	struct res_exec *re = (struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE

	struct dependency *dep;
	struct resource *other;
	char *key = res_exec_key(re);

	/* exec commands depend on their owner and group */
	if (ENFORCED(re, RES_EXEC_UID)) {
		other = policy_find_resource(pol, RES_USER, "username", re->user);
		if (other) {
			dep = dependency_new(key, other->key);
			if (policy_add_dependency(pol, dep) != 0) {
				dependency_free(dep);
				free(key);
				return -1;
			}
		}
	}

	if (ENFORCED(re, RES_EXEC_GID)) {
		other = policy_find_resource(pol, RES_GROUP, "name", re->group);
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
	return 0;
}

int res_exec_set(void *res, const char *name, const char *value)
{
	struct res_exec *re = (struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE

	if (strcmp(name, "user") == 0) {
		xfree(re->user);
		re->user = strdup(value);
		ENFORCE(re, RES_EXEC_UID);

	} else if (strcmp(name, "group") == 0) {
		xfree(re->group);
		re->group = strdup(value);
		ENFORCE(re, RES_EXEC_GID);

	} else if (strcmp(name, "command") == 0) {
		xfree(re->command);
		re->command = strdup(value);

	} else if (strcmp(name, "test") == 0) {
		xfree(re->test);
		re->test = strdup(value);
		ENFORCE(re, RES_EXEC_TEST);

	} else if (strcmp(name, "ondemand") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(re, RES_EXEC_ONDEMAND);
		} else {
			ENFORCE(re, RES_EXEC_ONDEMAND);
		}

	} else {
		return -1;

	}

	return 0;
}

int res_exec_match(const void *res, const char *name, const char *value)
{
	const struct res_exec *re = (struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE

	char *test_value;
	int rc;

	/* FIXME: match on key? */
	if (strcmp(name, "command") == 0) {
		test_value = string("%s", re->command);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

static int _res_exec_run(const char *command, uid_t uid, gid_t gid)
{
	int proc_stat;
	int null_in, null_out;
	pid_t pid;


	switch (pid = fork()) {
	case -1:
		/* fork failed */
		return -1;

	case 0: /* child */
		null_in  = open("/dev/null", O_RDONLY);
		null_out = open("/dev/null", O_WRONLY);

		dup2(null_in,  0);
		dup2(null_out, 1);
		dup2(null_out, 2);

		/* set user and group is */
		if (uid > 0) {
			if (setuid(uid) != 0) {
				WARNING("res_exec child could not switch to user ID %u to run `%s'", uid, command);
			} else {
				DEBUG("res_exec child set UID to %u", uid);
			}
		}

		if (gid > 0) {
			if (setgid(gid) != 0) {
				WARNING("res_exec child could not switch to group ID %u to run `%s'", uid, command);
			} else {
				DEBUG("res_exec child set GID to %u", gid);
			}
		}

		execl("/bin/sh", "sh", "-c", command, (char*)NULL);
		exit(42); /* Oops... exec failed */

	default: /* parent */
		DEBUG("res_exec: Running `%s' in sub-process %u", command, pid);
	}

	waitpid(pid, &proc_stat, 0);
	if (!WIFEXITED(proc_stat)) {
		DEBUG("res_exec[%u]: terminated abnormally", pid);
		return -1;
	}

	DEBUG("res_exec[%u]: sub-process exited %u", pid, WEXITSTATUS(proc_stat));
	return WEXITSTATUS(proc_stat);
}

int res_exec_stat(void *res, const struct resource_env *env)
{
	struct res_exec *re = (struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE
	assert(re->command); // LCOV_EXCL_LINE

	if (!re->uid && re->user) {
		assert(env);            // LCOV_EXCL_LINE
		assert(env->user_pwdb); // LCOV_EXCL_LINE
		re->uid = pwdb_lookup_uid(env->user_pwdb,  re->user);
	}
	if (!re->gid && re->group) {
		assert(env);             // LCOV_EXCL_LINE
		assert(env->group_grdb); // LCOV_EXCL_LINE
		re->gid = grdb_lookup_gid(env->group_grdb, re->group);
	}

	if (ENFORCED(re, RES_EXEC_TEST) && !ENFORCED(re, RES_EXEC_ONDEMAND)) {
		if (_res_exec_run(re->test, re->uid, re->gid) == 0) {
			ENFORCE(re, RES_EXEC_NEEDSRUN);
		} else {
			UNENFORCE(re, RES_EXEC_NEEDSRUN);
		}
	} else {
		ENFORCE(re, RES_EXEC_NEEDSRUN);
	}

	return 0;
}

struct report* res_exec_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_exec *re = (struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE
	assert(env); // LCOV_EXCL_LINE

	struct report *report = report_new("Run", re->command);
	char *action;

	if (ENFORCED(re, RES_EXEC_NEEDSRUN) || re->notified) {
		action = string("execute command");
		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (_res_exec_run(re->command, re->uid, re->gid) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	return report;
}

#define PACK_FORMAT "aLaaaa"
char *res_exec_pack(const void *res)
{
	const struct res_exec *re = (const struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE

	return pack("res_exec::", PACK_FORMAT,
		re->key, re->enforced,
		re->command, re->test, re->user, re->group);
}

void* res_exec_unpack(const char *packed)
{
	struct res_exec *re = res_exec_new(NULL);

	if (unpack(packed, "res_exec::", PACK_FORMAT,
		&re->key, &re->enforced,
		&re->command, &re->test, &re->user, &re->group) != 0) {

		res_exec_free(re);
		return NULL;
	}

	return re;
}
#undef PACK_FORMAT

int res_exec_notify(void *res, const struct resource *dep)
{
	struct res_exec *re = (struct res_exec*)(res);
	assert(re); // LCOV_EXCL)LINE

	re->notified = 1;

	return 0;
}


