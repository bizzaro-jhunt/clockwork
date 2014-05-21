/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

#include <fts.h>
#include <fcntl.h>
#include <libgen.h>
#include <sys/wait.h>
#include <time.h>

#define ENFORCE(r,f)   (r)->enforced  |=  (f)
#define UNENFORCE(r,f) (r)->enforced  &= ~(f)

#define _FD2FD_CHUNKSIZE 16384

static int _group_update(struct stringlist*, struct stringlist*, const char*);
static int _setup_path_deps(const char *key, const char *path, struct policy *pol);
static void _hash_attr(cw_hash_t *attrs, const char *key, void *val);

#define RES_DEFAULT(orig,field,dflt) ((orig) ? (orig)->field : (dflt))
#define RES_DEFAULT_STR(orig,field,dflt) cw_strdup(RES_DEFAULT((orig),field,(dflt)))

/*****************************************************************/

/* FIXME: need to manage home in PENDULUM code !!!
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
			home_path = cw_string("%s/%s", home, ent->fts_accpath);
			skel_path = cw_string("%s/%s", skel, ent->fts_accpath);
			mode = ent->fts_statp->st_mode & 0777;

			if (S_ISREG(ent->fts_statp->st_mode)) {
				skel_fd = open(skel_path, O_RDONLY);
				home_fd = creat(home_path, mode);

				if (chown(home_path, uid, gid) != 0) {
					_res_file_fd2fd(home_fd, skel_fd, -1);
				}
				if (skel_fd >= 0 && home_fd >= 0
				 && chown(home_path, uid, gid) == 0) {
					ssize_t n = 0;
					while (n >= 0 && (nread = read(skel_fd, buf, 8192)) > 0) {
						n = write(home_fd, buf, nread);
						// FIXME: handle write errors here
					}
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
*/

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

static int _setup_path_deps(const char *key, const char *spath, struct policy *pol)
{
	struct path *p;
	struct dependency *dep;
	struct resource *dir;

	cw_log(LOG_DEBUG, "setup_path_deps: setting up for %s (%s)", spath, key);
	p = path_new(spath);
	if (!p) {
		return -1;
	}
	if (path_canon(p) != 0) { goto failed; }

	while (path_pop(p) != 0) {
		/* look for deps, and add them. */
		cw_log(LOG_DEBUG, "setup_path_deps: looking for %s", path(p));
		dir = policy_find_resource(pol, RES_DIR, "path", path(p));
		if (dir) {
			dep = dependency_new(key, dir->key);
			if (policy_add_dependency(pol, dep) != 0) {
				dependency_free(dep);
				goto failed;
			}
		} else {
			cw_log(LOG_DEBUG, "setup_path_deps: no res_dir defined for '%s'", path(p));
		}
	}

	path_free(p);
	return 0;

failed:
	cw_log(LOG_DEBUG, "setup_path_deps: unspecified failure");
	path_free(p);
	return -1;
}

static void _hash_attr(cw_hash_t *attrs, const char *key, void *val)
{
	void *prev = cw_hash_get(attrs, key);
	cw_hash_set(attrs, key, val);
	if (prev) free(prev);
}


/*****************************************************************/

void* res_user_new(const char *key)
{
	return res_user_clone(NULL, key);
}

void* res_user_clone(const void *res, const char *key)
{
	struct res_user *orig = (struct res_user*)(res);
	struct res_user *ru = ru = cw_alloc(sizeof(struct res_user));

	ru->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

	ru->uid    = RES_DEFAULT(orig, uid,    -1);
	ru->gid    = RES_DEFAULT(orig, gid,    -1);
	ru->mkhome = RES_DEFAULT(orig, mkhome,  0);
	ru->lock   = RES_DEFAULT(orig, lock,    1);
	ru->pwmin  = RES_DEFAULT(orig, pwmin,   0);
	ru->pwmax  = RES_DEFAULT(orig, pwmax,   0);
	ru->pwwarn = RES_DEFAULT(orig, pwwarn,  0);
	ru->inact  = RES_DEFAULT(orig, inact,   0);
	ru->expire = RES_DEFAULT(orig, expire, -1);

	ru->name   = RES_DEFAULT_STR(orig, name,   NULL);
	ru->passwd = RES_DEFAULT_STR(orig, passwd, "*");
	ru->gecos  = RES_DEFAULT_STR(orig, gecos,  NULL);
	ru->shell  = RES_DEFAULT_STR(orig, shell,  NULL);
	ru->dir    = RES_DEFAULT_STR(orig, dir,    NULL);
	ru->skel   = RES_DEFAULT_STR(orig, skel,   NULL);

	ru->pw     = NULL;
	ru->sp     = NULL;

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
		free(ru->name);
		free(ru->passwd);
		free(ru->gecos);
		free(ru->dir);
		free(ru->shell);
		free(ru->skel);

		free(ru->key);
	}
	free(ru);
}

char* res_user_key(const void *res)
{
	const struct res_user *ru = (struct res_user*)(res);
	assert(ru); // LCOV_EXCL_LINE

	return cw_string("user:%s", ru->key);
}

int res_user_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_user *ru = (const struct res_user*)(res);
	assert(ru); // LCOV_EXCL_LINE

	_hash_attr(attrs, "username", strdup(ru->name));
	_hash_attr(attrs, "uid", ENFORCED(ru, RES_USER_UID) ? cw_string("%u",ru->uid) : NULL);
	_hash_attr(attrs, "gid", ENFORCED(ru, RES_USER_GID) ? cw_string("%u",ru->gid) : NULL);
	_hash_attr(attrs, "home", ENFORCED(ru, RES_USER_DIR) ? strdup(ru->dir) : NULL);
	_hash_attr(attrs, "present", strdup(ENFORCED(ru, RES_USER_ABSENT) ? "no" : "yes"));
	_hash_attr(attrs, "locked", ENFORCED(ru, RES_USER_LOCK) ? strdup(ru->lock ? "yes" : "no") : NULL);
	_hash_attr(attrs, "comment", ENFORCED(ru, RES_USER_GECOS) ? strdup(ru->gecos) : NULL);
	_hash_attr(attrs, "shell", ENFORCED(ru, RES_USER_SHELL) ? strdup(ru->shell) : NULL);
	_hash_attr(attrs, "password", ENFORCED(ru, RES_USER_PASSWD) ? strdup(ru->passwd) : NULL);
	_hash_attr(attrs, "pwmin", ENFORCED(ru, RES_USER_PWMIN) ? cw_string("%u", ru->pwmin) : NULL);
	_hash_attr(attrs, "pwmax", ENFORCED(ru, RES_USER_PWMAX) ? cw_string("%u", ru->pwmax) : NULL);
	_hash_attr(attrs, "pwwarn", ENFORCED(ru, RES_USER_PWWARN) ? cw_string("%u", ru->pwwarn) : NULL);
	_hash_attr(attrs, "inact", ENFORCED(ru, RES_USER_INACT) ? cw_string("%u", ru->inact) : NULL);
	_hash_attr(attrs, "expiration", ENFORCED(ru, RES_USER_EXPIRE) ? cw_string("%u", ru->expire) : NULL);
	_hash_attr(attrs, "skeleton", ENFORCED(ru, RES_USER_MKHOME) ? strdup(ru->skel) : NULL);
	return 0;
}

int res_user_norm(void *res, struct policy *pol, cw_hash_t *facts) { return 0; }

int res_user_set(void *res, const char *name, const char *value)
{
	struct res_user *ru = (struct res_user*)(res);
	assert(ru); // LCOV_EXCL_LINE

	if (strcmp(name, "uid") == 0) {
		ru->uid = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_UID);

	} else if (strcmp(name, "gid") == 0) {
		ru->gid = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_GID);

	} else if (strcmp(name, "username") == 0) {
		free(ru->name);
		ru->name = strdup(value);
		ENFORCE(ru, RES_USER_NAME);

	} else if (strcmp(name, "home") == 0) {
		free(ru->dir);
		ru->dir = strdup(value);
		ENFORCE(ru, RES_USER_DIR);

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") != 0) {
			UNENFORCE(ru, RES_USER_ABSENT);
		} else {
			ENFORCE(ru, RES_USER_ABSENT);
		}

	} else if (strcmp(name, "locked") == 0) {
		ru->lock = strcmp(value, "no") ? 1 : 0;
		ENFORCE(ru, RES_USER_LOCK);

	} else if (strcmp(name, "gecos") == 0 || strcmp(name, "comment") == 0) {
		free(ru->gecos);
		ru->gecos = strdup(value);
		ENFORCE(ru, RES_USER_GECOS);

	} else if (strcmp(name, "shell") == 0) {
		free(ru->shell);
		ru->shell = strdup(value);
		ENFORCE(ru, RES_USER_SHELL);

	} else if (strcmp(name, "pwhash") == 0 || strcmp(name, "password") == 0) {
		free(ru->passwd);
		ru->passwd = strdup(value);

	} else if (strcmp(name, "changepw") == 0) {
		if (strcmp(value, "no") == 0) {
			UNENFORCE(ru, RES_USER_PASSWD);
		} else {
			ENFORCE(ru, RES_USER_PASSWD);
		}

	} else if (strcmp(name, "pwmin") == 0) {
		ru->pwmin = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_PWMIN);

	} else if (strcmp(name, "pwmax") == 0) {
		ru->pwmax = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_PWMAX);

	} else if (strcmp(name, "pwwarn") == 0) {
		ru->pwwarn = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_PWWARN);

	} else if (strcmp(name, "inact") == 0) {
		ru->inact = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_INACT);

	} else if (strcmp(name, "expiry") == 0 || strcmp(name, "expiration") == 0) {
		ru->expire = strtoll(value, NULL, 10);
		ENFORCE(ru, RES_USER_EXPIRE);

	} else if (strcmp(name, "skeleton") == 0 || strcmp(name, "makehome") == 0) {
		ENFORCE(ru, RES_USER_MKHOME);
		free(ru->skel); ru->skel = NULL;
		ru->mkhome = (strcmp(value, "no") ? 1 : 0);

		if (!ru->mkhome) { return 0; }
		ru->skel = strdup(strcmp(value, "yes") == 0 ? "/etc/skel" : value);

	} else {
		/* unknown attribute. */
		return -1;

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
		test_value = cw_string("%u", ru->uid);
	} else if (strcmp(name, "gid") == 0) {
		test_value = cw_string("%u", ru->gid);
	} else if (strcmp(name, "username") == 0) {
		test_value = cw_string("%s", ru->name);
	} else if (strcmp(name, "home") == 0) {
		test_value = cw_string("%s", ru->dir);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_user_gencode(const void *res, FILE *io, unsigned int next)
{
	struct res_user *r = (struct res_user*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, ";; res_user %s\n", r->key);
	fprintf(io, "SET %%A 1\n");
	fprintf(io, "SET %%B \"%s\"\n", r->name);
	fprintf(io, "CALL &USER.FIND\n");

	if (ENFORCED(r, RES_USER_ABSENT)) {
		fprintf(io, "OK? @next.%i\n", next);
		fprintf(io, "  CALL &USER.REMOVE\n");
	} else {
		fprintf(io, "OK? @create.%i\n", next);
		fprintf(io, "  JUMP @check.ids.%i\n", next);
		fprintf(io, "create.%i:\n", next);
		if (ENFORCED(r, RES_USER_GID)) {
			fprintf(io, "SET %%C %i\n", r->gid);
		} else {
			fprintf(io, "SET %%A 1\n");
			fprintf(io, "SET %%B \"%s\"\n", r->name);
			fprintf(io, "CALL &GROUP.FIND\n");
			fprintf(io, "OK? @group.found.%i\n", next);
			fprintf(io, "  COPY %%B %%A\n");
			fprintf(io, "  CALL &GROUP.NEXT_GID\n");
			fprintf(io, "  COPY %%R %%B\n");
			fprintf(io, "  COPY %%R %%C\n");
			fprintf(io, "  CALL &GROUP.CREATE\n");
			fprintf(io, "  JUMP @group.done.%i\n", next);
			fprintf(io, "group.found.%i:\n", next);
			fprintf(io, "  CALL &GROUP.GET_GID\n");
			fprintf(io, "  COPY %%R %%C\n");
			fprintf(io, "group.done.%i:\n", next);
		}
		if (ENFORCED(r, RES_USER_UID)) {
			fprintf(io, "SET %%B %i\n", r->uid);
		} else {
			fprintf(io, "CALL &USER.NEXT_UID\n");
			fprintf(io, "COPY %%R %%B\n");
		}
		fprintf(io, "CALL &USER.CREATE\n");
		if (r->passwd && !ENFORCED(r, RES_USER_PASSWD)) {
			fprintf(io, "SET %%B \"x\"\n");
			fprintf(io, "CALL &USER.SET_PASSWD\n");
			fprintf(io, "SET %%B \"%s\"\n", r->passwd);
			fprintf(io, "CALL &USER.SET_PWHASH\n");
		}
		fprintf(io, "JUMP @exists.%i\n", next);
		fprintf(io, "check.ids.%i:\n", next);
		if (ENFORCED(r, RES_USER_UID)) {
			fprintf(io, "SET %%B %i\n", r->uid);
			fprintf(io, "CALL &USER.SET_UID\n");
		}
		if (ENFORCED(r, RES_USER_GID)) {
			fprintf(io, "SET %%B %i\n", r->gid);
			fprintf(io, "CALL &USER.SET_GID\n");
		}
		fprintf(io, "exists.%i:\n", next);

		if (ENFORCED(r, RES_USER_PASSWD)) {
			fprintf(io, "SET %%B \"x\"\n");
			fprintf(io, "CALL &USER.SET_PASSWD\n");
			fprintf(io, "SET %%B \"%s\"\n", r->passwd);
			fprintf(io, "CALL &USER.SET_PWHASH\n");
		}

		if (ENFORCED(r, RES_USER_GECOS)) {
			fprintf(io, "SET %%B \"%s\"\n", r->gecos);
			fprintf(io, "CALL &USER.SET_GECOS\n");
		}
		if (ENFORCED(r, RES_USER_SHELL)) {
			fprintf(io, "SET %%B \"%s\"\n", r->shell);
			fprintf(io, "CALL &USER.SET_SHELL\n");
		}
		if (ENFORCED(r, RES_USER_PWMIN)) {
			fprintf(io, "SET %%B %li\n", r->pwmin);
			fprintf(io, "CALL &USER.SET_PWMIN\n");
		}
		if (ENFORCED(r, RES_USER_PWMAX)) {
			fprintf(io, "SET %%B %li\n", r->pwmax);
			fprintf(io, "CALL &USER.SET_PWMAX\n");
		}
		if (ENFORCED(r, RES_USER_PWWARN)) {
			fprintf(io, "SET %%B %li\n", r->pwwarn);
			fprintf(io, "CALL &USER.SET_PWWARN\n");
		}
		if (ENFORCED(r, RES_USER_INACT)) {
			fprintf(io, "SET %%B %li\n", r->inact);
			fprintf(io, "CALL &USER.SET_INACT\n");
		}
		if (ENFORCED(r, RES_USER_EXPIRE)) {
			fprintf(io, "SET %%B %li\n", r->expire);
			fprintf(io, "CALL &USER.SET_EXPIRY\n");
		}
	}

	return 0;
}

FILE * res_user_content(const void *res, cw_hash_t *facts) { return NULL; }

int res_user_notify(void *res, const struct resource *dep) { return 0; }



/*****************************************************************/

void* res_file_new(const char *key)
{
	return res_file_clone(NULL, key);
}

void* res_file_clone(const void *res, const char *key)
{
	const struct res_file *orig = (const struct res_file*)(res);
	struct res_file *rf = cw_alloc(sizeof(struct res_file));

	rf->enforced    = RES_DEFAULT(orig, enforced,  RES_NONE);

	rf->owner    = RES_DEFAULT_STR(orig, owner, NULL);
	rf->group    = RES_DEFAULT_STR(orig, group, NULL);

	rf->uid      = RES_DEFAULT(orig, uid, 0);
	rf->gid      = RES_DEFAULT(orig, gid, 0);

	rf->mode     = RES_DEFAULT(orig, mode, 0600);

	rf->path     = RES_DEFAULT_STR(orig, path, NULL);
	rf->source   = RES_DEFAULT_STR(orig, source, NULL);
	rf->template = RES_DEFAULT_STR(orig, template, NULL);

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
		free(rf->source);
		free(rf->path);
		free(rf->template);
		free(rf->owner);
		free(rf->group);
		free(rf->key);
	}

	free(rf);
}

char* res_file_key(const void *res)
{
	const struct res_file *rf = (struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

	return cw_string("file:%s", rf->key);
}

#define DUMP_UNSPEC(io,s) fprintf(io, "# %s unspecified\n", s)

int res_file_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_file *rf = (const struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

	_hash_attr(attrs, "path", strdup(rf->path));
	_hash_attr(attrs, "present", strdup(ENFORCED(rf, RES_FILE_ABSENT) ? "no" : "yes"));

	_hash_attr(attrs, "owner", ENFORCED(rf, RES_FILE_UID) ? strdup(rf->owner) : NULL);
	_hash_attr(attrs, "group", ENFORCED(rf, RES_FILE_GID) ? strdup(rf->group) : NULL);
	_hash_attr(attrs, "mode", ENFORCED(rf, RES_FILE_MODE) ? cw_string("%04o", rf->mode) : NULL);

	if (ENFORCED(rf, RES_FILE_SHA1)) {
		_hash_attr(attrs, "template", cw_strdup(rf->template));
		_hash_attr(attrs, "source",   cw_strdup(rf->source));
	} else {
		_hash_attr(attrs, "template", NULL);
		_hash_attr(attrs, "source",   NULL);
	}

	return 0;
}

int res_file_norm(void *res, struct policy *pol, cw_hash_t *facts)
{
	struct res_file *rf = (struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

	struct dependency *dep;
	struct resource *other;
	char *key = res_file_key(rf);

	/* files depend on their owner and group */
	if (ENFORCED(rf, RES_FILE_UID)) {
		other = policy_find_resource(pol, RES_USER, "username", rf->owner);

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
		other = policy_find_resource(pol, RES_GROUP, "name", rf->group);
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
	if (_setup_path_deps(key, rf->path, pol) != 0) {
		free(key);
		return -1;
	}

	free(key);
	return 0;
}

int res_file_set(void *res, const char *name, const char *value)
{
	struct res_file *rf = (struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

	if (strcmp(name, "owner") == 0) {
		free(rf->owner);
		rf->owner = strdup(value);
		ENFORCE(rf, RES_FILE_UID);

	} else if (strcmp(name, "group") == 0) {
		free(rf->group);
		rf->group = strdup(value);
		ENFORCE(rf, RES_FILE_GID);

	} else if (strcmp(name, "mode") == 0) {
		/* Mask off non-permission bits */
		rf->mode = strtoll(value, NULL, 0) & 07777;
		ENFORCE(rf, RES_FILE_MODE);

	} else if (strcmp(name, "source") == 0) {
		free(rf->template);
		rf->template = NULL;
		free(rf->source);
		rf->source = strdup(value);
		ENFORCE(rf, RES_FILE_SHA1);

	} else if (strcmp(name, "template") == 0) {
		free(rf->source);
		rf->source = NULL;
		free(rf->template);
		rf->template = strdup(value);
		ENFORCE(rf, RES_FILE_SHA1);

	} else if (strcmp(name, "path") == 0) {
		free(rf->path);
		rf->path = strdup(value);

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
		test_value = cw_string("%s", rf->path);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_file_gencode(const void *res, FILE *io, unsigned int next)
{
	struct res_file *r = (struct res_file*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, ";; res_file %s\n", r->key);
	fprintf(io, "SET %%A \"%s\"\n", r->path);

	if (ENFORCED(r, RES_FILE_ABSENT)) {
		fprintf(io, "CALL &FS.EXISTS?\n");
		fprintf(io, "OK? @next.%i\n", next);
		fprintf(io, "  CALL &FS.UNLINK\n");
		fprintf(io, "  JUMP @next.%i\n", next);
		return 0;
	}

	fprintf(io, "CALL &FS.EXISTS?\n");
	fprintf(io, "OK? @create.%i\n", next);
	fprintf(io, "  JUMP @exists.%i\n", next);
	fprintf(io, "create.%i:\n", next);
	fprintf(io, "  CALL &FS.MKFILE\n");
	fprintf(io, "exists.%i:\n", next);

	if (ENFORCED(r, RES_FILE_UID) || ENFORCED(r, RES_FILE_GID)) {
		fprintf(io, "COPY %%A %%F\n");
		fprintf(io, "SET %%D 0\n");
		fprintf(io, "SET %%E 0\n");

		if (ENFORCED(r, RES_FILE_UID)) {
			fprintf(io, "SET %%A 1\n");
			fprintf(io, "SET %%B \"%s\"\n", r->owner);
			fprintf(io, "CALL &USER.FIND\n");
			fprintf(io, "NOTOK? @found.user.%i\n", next);
			fprintf(io, "  COPY %%B %%A\n");
			fprintf(io, "  PRINT \"Unable to find user '%%s'\\n\"\n");
			fprintf(io, "  JUMP @userfind.done.%i\n", next);
			fprintf(io, "found.user.%i:\n", next);
			fprintf(io, "CALL &USER.GET_UID\n");
			fprintf(io, "COPY %%R %%D\n");
			fprintf(io, "userfind.done.%i:\n", next);
		}

		if (ENFORCED(r, RES_FILE_GID)) {
			fprintf(io, "SET %%A 1\n");
			fprintf(io, "SET %%B \"%s\"\n", r->group);
			fprintf(io, "CALL &GROUP.FIND\n");
			fprintf(io, "NOTOK? @found.group.%i\n", next);
			fprintf(io, "  COPY %%B %%A\n");
			fprintf(io, "  PRINT \"Unable to find group '%%s'\\n\"\n");
			fprintf(io, "  JUMP @groupfind.done.%i\n", next);
			fprintf(io, "found.group.%i:\n", next);
			fprintf(io, "CALL &GROUP.GET_GID\n");
			fprintf(io, "COPY %%R %%E\n");
			fprintf(io, "groupfind.done.%i:\n", next);
		}
		fprintf(io, "COPY %%F %%A\n");
		fprintf(io, "COPY %%D %%B\n");
		fprintf(io, "COPY %%E %%C\n");
		fprintf(io, "CALL &FS.CHOWN\n");
	}

	if (ENFORCED(r, RES_FILE_MODE)) {
		fprintf(io, "SET %%D 0%o\n", r->mode);
		fprintf(io, "CALL &FS.CHMOD\n");
	}

	if (ENFORCED(r, RES_FILE_SHA1)) {
		fprintf(io, "CALL &FS.SHA1\n");
		fprintf(io, "OK? @content.fail.%i\n", next);
		fprintf(io, "  COPY %%S2 %%T1\n");
		fprintf(io, "  COPY %%A %%F\n");
		fprintf(io, "  SET %%A \"file:%s\"\n", r->key);
		fprintf(io, "  CALL &SERVER.SHA1\n");
		fprintf(io, "  COPY %%F %%A\n");
		fprintf(io, "  OK? @content.fail.%i\n", next);
		fprintf(io, "    COPY %%S2 %%T2\n");
		fprintf(io, "    CMP? @diff.%i\n", next);
		fprintf(io, "      JUMP @sha1.done.%i\n", next);

		fprintf(io, "diff.%i:\n", next);
		fprintf(io, "CALL &SERVER.WRITEFILE\n");
		fprintf(io, "OK? @content.fail.%i\n", next);
		fprintf(io, "  JUMP @sha1.done.%i\n", next);

		fprintf(io, "content.fail.%i:\n", next);
		fprintf(io, "PRINT \"Failed to update contents of %%s\\n\"\n");

		fprintf(io, "sha1.done.%i:\n", next);
	}

	return 0;
}

FILE * res_file_content(const void *res, cw_hash_t *facts)
{
	struct res_file *r = (struct res_file*)(res);
	assert(r); // LCOV_EXCL_LINE

	if (r->template) {
		return cw_tpl_erb(r->template, facts);

	} else if (r->source) {
		return fopen(r->source, "r");
	}
	return NULL;
}

int res_file_notify(void *res, const struct resource *dep) { return 0; }

/*****************************************************************/

void* res_group_new(const char *key)
{
	return res_group_clone(NULL, key);
}

void* res_group_clone(const void *res, const char *key)
{
	const struct res_group *orig = (const struct res_group*)(res);
	struct res_group *rg = cw_alloc(sizeof(struct res_group));

	rg->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

	rg->name   = RES_DEFAULT_STR(orig, name,   NULL);
	rg->passwd = RES_DEFAULT_STR(orig, passwd, NULL);

	rg->gid    = RES_DEFAULT(orig, gid, 0);

	/* FIXME: clone members of res_group */
	rg->mem_add = stringlist_new(NULL);
	rg->mem_rm  = stringlist_new(NULL);

	/* FIXME: clone admins of res_group */
	rg->adm_add = stringlist_new(NULL);
	rg->adm_rm  = stringlist_new(NULL);

	/* state variables are never cloned */
	rg->grp = NULL;
	rg->sg  = NULL;
	rg->mem = NULL;
	rg->adm = NULL;

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

		free(rg->name);
		free(rg->passwd);

		if (rg->mem) {
			stringlist_free(rg->mem);
		}
		stringlist_free(rg->mem_add);
		stringlist_free(rg->mem_rm);

		if (rg->adm) {
			stringlist_free(rg->adm);
		}
		stringlist_free(rg->adm_add);
		stringlist_free(rg->adm_rm);
	}
	free(rg);
}

char* res_group_key(const void *res)
{
	const struct res_group *rg = (struct res_group*)(res);
	assert(rg); // LCOV_EXCL_LINE

	return cw_string("group:%s", rg->key);
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
		final = cw_string("%s !%s", added, removed);
		free(added);
		free(removed);
		added = removed = NULL;

	} else if (added) {
		final = added;

	} else if (removed) {
		final = cw_string("!%s", removed);
		free(removed);
		removed = NULL;

	} else {
		final = strdup("");

	}
	return final;
}

int res_group_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_group *rg = (const struct res_group*)(res);
	assert(rg); // LCOV_EXCL_LINE

	_hash_attr(attrs, "name", strdup(rg->name));
	_hash_attr(attrs, "gid", ENFORCED(rg, RES_GROUP_GID) ? cw_string("%u",rg->gid) : NULL);
	_hash_attr(attrs, "present", strdup(ENFORCED(rg, RES_GROUP_ABSENT) ? "no" : "yes"));
	_hash_attr(attrs, "password", ENFORCED(rg, RES_GROUP_PASSWD) ? strdup(rg->passwd) : NULL);
	if (ENFORCED(rg, RES_GROUP_MEMBERS)) {
		_hash_attr(attrs, "members", _res_group_roster_mv(rg->mem_add, rg->mem_rm));
	} else {
		_hash_attr(attrs, "members", NULL);
	}
	if (ENFORCED(rg, RES_GROUP_ADMINS)) {
		_hash_attr(attrs, "admins", _res_group_roster_mv(rg->adm_add, rg->adm_rm));
	} else {
		_hash_attr(attrs, "admins", NULL);
	}
	return 0;
}

int res_group_norm(void *res, struct policy *pol, cw_hash_t *facts) { return 0; }

int res_group_set(void *res, const char *name, const char *value)
{
	struct res_group *rg = (struct res_group*)(res);

	/* for multi-value attributes */
	struct stringlist *multi;
	size_t i;

	assert(rg); // LCOV_EXCL_LINE

	if (strcmp(name, "gid") == 0) {
		rg->gid = strtoll(value, NULL, 10);
		ENFORCE(rg, RES_GROUP_GID);

	} else if (strcmp(name, "name") == 0) {
		free(rg->name);
		rg->name = strdup(value);
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
		free(rg->passwd);
		rg->passwd = strdup(value);

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
		test_value = cw_string("%u", rg->gid);
	} else if (strcmp(name, "name") == 0) {
		test_value = cw_string("%s", rg->name);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_group_gencode(const void *res, FILE *io, unsigned int next)
{
	struct res_group *r = (struct res_group*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, ";; res_group %s\n", r->key);
	fprintf(io, "SET %%A 1\n");
	fprintf(io, "SET %%B \"%s\"\n", r->name);
	fprintf(io, "CALL &GROUP.FIND\n");

	if (ENFORCED(r, RES_GROUP_ABSENT)) {
		fprintf(io, "OK? @next.%i\n", next);
		fprintf(io, "  CALL &GROUP.REMOVE\n");
	} else {
		fprintf(io, "OK? @found.%i\n", next);
		fprintf(io, "  COPY %%B %%A\n");

		if (ENFORCED(r, RES_GROUP_GID)) {
			fprintf(io, "  SET %%B %i\n", r->gid);
		} else {
			fprintf(io, "  CALL &GROUP.NEXT_GID\n");
			fprintf(io, "  COPY %%R %%B\n");
		}

		fprintf(io, "  CALL &GROUP.CREATE\n");
		fprintf(io, "  JUMP @update.%i\n", next);
		fprintf(io, "found.%i:\n", next);

		if (ENFORCED(r, RES_GROUP_GID)) {
			fprintf(io, "  SET %%B %i\n", r->gid);
			fprintf(io, "  CALL &GROUP.SET_GID\n");
		}

		fprintf(io, "update.%i:\n", next);

		if (ENFORCED(r, RES_GROUP_PASSWD)) {
			fprintf(io, "SET %%B \"x\"\n");
			fprintf(io, "CALL &GROUP.SET_PASSWD\n");
			fprintf(io, "SET %%B \"%s\"\n", r->passwd);
			fprintf(io, "CALL &GROUP.SET_PWHASH\n");
		}

		if (ENFORCED(r, RES_GROUP_MEMBERS)) {
			fprintf(io, ";; members\n");
			char ** name;
			for (name = r->mem_add->strings; *name; name++) {
				fprintf(io, "SET %%A \"%s\"\n", *name);
				fprintf(io, "CALL &GROUP.HAS_MEMBER?\n");
				fprintf(io, "OK? @member.%s.else.%i\n", *name, next);
				fprintf(io, "  CALL &GROUP.ADD_MEMBER\n");
				fprintf(io, "member.%s.else.%i:\n", *name, next);
			}
			for (name = r->mem_rm->strings; *name; name++) {
				fprintf(io, "SET %%A \"%s\"\n", *name);
				fprintf(io, "CALL &GROUP.HAS_MEMBER?\n");
				fprintf(io, "NOTOK? @member.%s.else.%i\n", *name, next);
				fprintf(io, "  CALL &GROUP.RM_MEMBER\n");
				fprintf(io, "member.%s.else.%i:\n", *name, next);
			}
		}

		if (ENFORCED(r, RES_GROUP_ADMINS)) {
			fprintf(io, ";; admins\n");
			char ** name;
			for (name = r->adm_add->strings; *name; name++) {
				fprintf(io, "SET %%A \"%s\"\n", *name);
				fprintf(io, "CALL &GROUP.HAS_ADMIN?\n");
				fprintf(io, "OK? @admin.%s.else.%i\n", *name, next);
				fprintf(io, "  CALL &GROUP.ADD_ADMIN\n");
				fprintf(io, "admin.%s.else.%i:\n", *name, next);
			}
			for (name = r->adm_rm->strings; *name; name++) {
				fprintf(io, "SET %%A \"%s\"\n", *name);
				fprintf(io, "CALL &GROUP.HAS_ADMIN?\n");
				fprintf(io, "NOTOK? @admin.%s.else.%i\n", *name, next);
				fprintf(io, "  CALL &GROUP.RM_ADMIN\n");
				fprintf(io, "admin.%s.else.%i:\n", *name, next);
			}
		}
	}

	return 0;
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
	/* add to mem_add, remove from mem_rm */
	return _group_update(rg->mem_add, rg->mem_rm, user);
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
	/* add to mem_rm, remove from mem_add */
	return _group_update(rg->mem_rm, rg->mem_add, user);
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
	/* add to adm_add, remove from adm_rm */
	return _group_update(rg->adm_add, rg->adm_rm, user);
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
	/* add to adm_rm, remove from adm_add */
	return _group_update(rg->adm_rm, rg->adm_add, user);
}

FILE * res_group_content(const void *res, cw_hash_t *facts) { return NULL; }

int res_group_notify(void *res, const struct resource *dep) { return 0; }

/*****************************************************************/

void* res_package_new(const char *key)
{
	return res_package_clone(NULL, key);
}

void* res_package_clone(const void *res, const char *key)
{
	const struct res_package *orig = (const struct res_package*)(res);
	struct res_package *rp = cw_alloc(sizeof(struct res_package));

	rp->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

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
		free(rp->installed);

		free(rp->key);
	}

	free(rp);
}

char* res_package_key(const void *res)
{
	const struct res_package *rp = (struct res_package*)(res);
	assert(rp); // LCOV_EXCL_LINE

	return cw_string("package:%s", rp->key);
}

int res_package_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_package *rp = (const struct res_package*)(res);
	assert(rp); // LCOV_EXCL_LINE

	_hash_attr(attrs, "name", cw_strdup(rp->name));
	_hash_attr(attrs, "version", cw_strdup(rp->version));
	_hash_attr(attrs, "installed", strdup(ENFORCED(rp, RES_PACKAGE_ABSENT) ? "no" : "yes"));
	return 0;
}

int res_package_norm(void *res, struct policy *pol, cw_hash_t *facts) { return 0; }

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
		test_value = cw_string("%s", rp->name);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_package_gencode(const void *res, FILE *io, unsigned int next)
{
	struct res_package *r = (struct res_package*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, ";; res_package %s\n", r->key);
	if (ENFORCED(r, RES_PACKAGE_ABSENT)) {
		fprintf(io, "SET %%A \"cwtool package remove %s\"\n", r->name);
	} else {
		fprintf(io, "SET %%A \"cwtool package install %s %s\"\n", r->name, r->version ? r->version : "latest");
	}
	fprintf(io, "CALL &EXEC.CHECK\n");
	return 0;
}

FILE * res_package_content(const void *res, cw_hash_t *facts) { return NULL; }

int res_package_notify(void *res, const struct resource *dep) { return 0; }

/*****************************************************************/

void* res_service_new(const char *key)
{
	return res_service_clone(NULL, key);
}

void* res_service_clone(const void *res, const char *key)
{
	const struct res_service *orig = (const struct res_service*)(res);
	struct res_service *rs = cw_alloc(sizeof(struct res_service));

	rs->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

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

	return cw_string("service:%s", rs->key);
}

int res_service_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_service *rs = (const struct res_service*)(res);
	assert(rs); // LCOV_EXCL_LINE

	_hash_attr(attrs, "name", cw_strdup(rs->service));
	_hash_attr(attrs, "running", strdup(ENFORCED(rs, RES_SERVICE_RUNNING) ? "yes" : "no"));
	_hash_attr(attrs, "enabled", strdup(ENFORCED(rs, RES_SERVICE_ENABLED) ? "yes" : "no"));
	return 0;
}

int res_service_norm(void *res, struct policy *pol, cw_hash_t *facts) { return 0; }

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
		test_value = cw_string("%s", rs->service);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_service_gencode(const void *res, FILE *io, unsigned int next)
{
	struct res_service *r = (struct res_service*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, ";; res_service %s\n", r->key);
	if (ENFORCED(r, RES_SERVICE_ENABLED)) {
		fprintf(io, "SET %%A \"cwtool service enable %s\"\n", r->service);
		fprintf(io, "CALL &EXEC.CHECK\n");

	} else if (ENFORCED(r, RES_SERVICE_DISABLED)) {
		fprintf(io, "SET %%A \"cwtool service disable %s\"\n", r->service);
		fprintf(io, "CALL &EXEC.CHECK\n");
	}

	if (ENFORCED(r, RES_SERVICE_RUNNING)) {
		fprintf(io, "SET %%A \"cwtool service start %s\"\n", r->service);
		fprintf(io, "CALL &EXEC.CHECK\n");

	} else if (ENFORCED(r, RES_SERVICE_STOPPED)) {
		fprintf(io, "SET %%A \"cwtool service stop %s\"\n", r->service);
		fprintf(io, "CALL &EXEC.CHECK\n");
	}

	return 0;
}

FILE * res_service_content(const void *res, cw_hash_t *facts) { return NULL; }

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
	struct res_host *rh = cw_alloc(sizeof(struct res_host));

	rh->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

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
		free(rh->aug_root);
		free(rh->hostname);
		free(rh->ip);
		stringlist_free(rh->aliases);

		free(rh->key);
	}

	free(rh);
}

char* res_host_key(const void *res)
{
	const struct res_host *rh = (struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE

	return cw_string("host:%s", rh->key);
}

int res_host_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_host *rh = (const struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE

	_hash_attr(attrs, "hostname", cw_strdup(rh->hostname));
	_hash_attr(attrs, "present",  strdup(ENFORCED(rh, RES_HOST_ABSENT) ? "no" : "yes"));
	_hash_attr(attrs, "ip", cw_strdup(rh->ip));
	if (ENFORCED(rh, RES_HOST_ALIASES)) {
		_hash_attr(attrs, "aliases", stringlist_join(rh->aliases, " "));
	} else {
		_hash_attr(attrs, "aliases", NULL);
	}
	return 0;
}

int res_host_norm(void *res, struct policy *pol, cw_hash_t *facts) { return 0; }

int res_host_set(void *res, const char *name, const char *value)
{
	struct res_host *rh = (struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE
	struct stringlist *alias_tmp;

	if (strcmp(name, "hostname") == 0) {
		free(rh->hostname);
		rh->hostname = strdup(value);

	} else if (strcmp(name, "ip") == 0 || strcmp(name, "address") == 0) {
		free(rh->ip);
		rh->ip = strdup(value);

	} else if (strcmp(name, "aliases") == 0 || strcmp(name, "alias") == 0) {
		alias_tmp = stringlist_split(value, strlen(value), " ", SPLIT_GREEDY);
		if (stringlist_add_all(rh->aliases, alias_tmp) != 0) {
			stringlist_free(alias_tmp);
			return -1;
		}
		stringlist_free(alias_tmp);

		ENFORCE(rh, RES_HOST_ALIASES);

	} else if (strcmp(name, "present") == 0) {
		if (strcmp(value, "no") == 0) {
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
		test_value = cw_string("%s", rh->hostname);
	} else if (strcmp(name, "ip") == 0 || strcmp(name, "address") == 0) {
		test_value = cw_string("%s", rh->ip);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_host_gencode(const void *res, FILE *io, unsigned int next)
{
	struct res_host *r = (struct res_host*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, ";; res_host %s\n", r->key);
	fprintf(io, "SET %%A \"/files/etc/hosts/*[ipaddr = \\\"%s\\\" and canonical = \\\"%s\\\"]\"\n", r->ip, r->hostname);
	fprintf(io, "CALL &AUGEAS.FIND\n");

	if (ENFORCED(r, RES_HOST_ABSENT)) {
		fprintf(io, "OK @not.found.%i\n", next);
		fprintf(io, "  COPY %%R %%A\n");
		fprintf(io, "  CALL &AUGEAS.REMOVE\n");
		fprintf(io, "not.found.%i:\n", next);

	} else {
		fprintf(io, "NOTOK @found.%i\n", next);
		fprintf(io, "  SET %%A \"/files/etc/hosts/%i/ipaddr\"\n", 99999+next);
		fprintf(io, "  SET %%B \"%s\"\n", r->ip);
		fprintf(io, "  CALL &AUGEAS.SET\n");
		fprintf(io, "  SET %%A \"/files/etc/hosts/%i/canonical\"\n", 99999+next);
		fprintf(io, "  SET %%B \"%s\"\n", r->hostname);
		fprintf(io, "  CALL &AUGEAS.SET\n");
		fprintf(io, "  JUMP @aliases.%i\n", next);
		fprintf(io, "found.%i:\n", next);
		fprintf(io, "  COPY %%S2 %%A\n");

		if (ENFORCED(r, RES_HOST_ALIASES)) {
			fprintf(io, "SET %%C \"/alias\"\n");
			fprintf(io, "CALL &AUGEAS.REMOVE\n");

			int i;
			for (i = 0; i < r->aliases->num; i++) {
				fprintf(io, "SET %%C \"/alias[%u]\"\n", i);
				fprintf(io, "SET %%B \"%s\"\n", r->aliases->strings[i]);
				fprintf(io, "CALL &AUGEAS.SET\n");
			}
		}
	}


	return 0;
}

FILE * res_host_content(const void *res, cw_hash_t *facts) { return NULL; }

int res_host_notify(void *res, const struct resource *dep) { return 0; }

/*****************************************************************/

void* res_sysctl_new(const char *key)
{
	return res_sysctl_clone(NULL, key);
}

void* res_sysctl_clone(const void *res, const char *key)
{
	const struct res_sysctl *orig = (const struct res_sysctl*)(res);
	struct res_sysctl *rs = cw_alloc(sizeof(struct res_sysctl));

	rs->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

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

	return cw_string("sysctl:%s", rs->key);
}

int res_sysctl_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_sysctl *rs = (const struct res_sysctl*)(res);
	assert(rs); // LCOV_EXCL_LINE

	_hash_attr(attrs, "param", cw_strdup(rs->param));
	_hash_attr(attrs, "value", ENFORCED(rs, RES_SYSCTL_VALUE) ? strdup(rs->value) : NULL);
	_hash_attr(attrs, "persist", strdup(ENFORCED(rs, RES_SYSCTL_PERSIST) ? "yes" : "no"));
	return 0;
}

int res_sysctl_norm(void *res, struct policy *pol, cw_hash_t *facts) { return 0; }

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
		test_value = cw_string("%s", rs->param);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_sysctl_gencode(const void *res, FILE *io, unsigned int next)
{
	struct res_sysctl *r = (struct res_sysctl*)(res);
	assert(r); // LCOV_EXCL_LINE

	char *p, *path = strdup(r->param);
	for (p = path; *p; p++)
		if (*p == '.') *p = '/';

	fprintf(io, ";; res_sysctl %s\n", r->key);
	if (ENFORCED(r, RES_SYSCTL_VALUE)) {
		fprintf(io, "SET %%A \"/proc/sys/%s\"\n", path);
		fprintf(io, "CALL &FS.GET\n");
		fprintf(io, "COPY %%S2 %%T1\n");
		fprintf(io, "SET %%T2 \"%s\"\n", r->value);
		fprintf(io, "CMP? @diff.%i\n", next);
		fprintf(io, "  JUMP @done.%i\n", next);
		fprintf(io, "diff.%i:\n", next);
		fprintf(io, "  COPY %%T2 %%B\n");
		fprintf(io, "  CALL &FS.PUT\n");
		fprintf(io, "done.%i:\n", next);

		if (ENFORCED(r, RES_SYSCTL_PERSIST)) {
			fprintf(io, "SET %%A \"/files/etc/sysctl.conf/%%s\"\n");
			fprintf(io, "SET %%B \"%s\"\n", r->value);
			fprintf(io, "SET %%C \"%s\"\n", r->param);
			fprintf(io, "CALL &AUGEAS.SET\n");
		}
	}

	free(path);
	return 0;
}

FILE * res_sysctl_content(const void *res, cw_hash_t *facts) { return NULL; }

int res_sysctl_notify(void* res, const struct resource *dep) { return 0; }

/*****************************************************************/

void* res_dir_new(const char *key)
{
	return res_dir_clone(NULL, key);
}

void* res_dir_clone(const void *res, const char *key)
{
	const struct res_dir *orig = (const struct res_dir*)(res);
	struct res_dir *rd = cw_alloc(sizeof(struct res_dir));

	rd->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

	rd->owner = RES_DEFAULT_STR(orig, owner, NULL);
	rd->group = RES_DEFAULT_STR(orig, group, NULL);

	rd->uid   = RES_DEFAULT(orig, uid, 0);
	rd->gid   = RES_DEFAULT(orig, gid, 0);
	rd->mode  = RES_DEFAULT(orig, mode, 0700);

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

	return cw_string("dir:%s", rd->key);
}

int res_dir_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_dir *rd = (const struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE

	_hash_attr(attrs, "path", cw_strdup(rd->path));
	_hash_attr(attrs, "owner", ENFORCED(rd, RES_DIR_UID) ? strdup(rd->owner) : NULL);
	_hash_attr(attrs, "group", ENFORCED(rd, RES_DIR_GID) ? strdup(rd->group) : NULL);
	_hash_attr(attrs, "mode", ENFORCED(rd, RES_DIR_MODE) ? cw_string("%04o", rd->mode) : NULL);
	_hash_attr(attrs, "present", strdup(ENFORCED(rd, RES_DIR_ABSENT) ? "no" : "yes"));
	return 0;
}

int res_dir_norm(void *res, struct policy *pol, cw_hash_t *facts)
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
		test_value = cw_string("%s", rd->path);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_dir_gencode(const void *res, FILE *io, unsigned int next)
{
	struct res_dir *r = (struct res_dir*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, ";; res_dir %s\n", r->key);
	fprintf(io, "SET %%A \"%s\"\n", r->path);

	if (ENFORCED(r, RES_DIR_ABSENT)) {
		fprintf(io, "CALL &FS.EXISTS?\n");
		fprintf(io, "OK? @next.%i\n", next);
		fprintf(io, "  CALL &FS.RMDIR\n");
		fprintf(io, "  JUMP @next.%i\n", next);
		return 0;
	}

	fprintf(io, "CALL &FS.EXISTS?\n");
	fprintf(io, "OK? @create.%i\n", next);
	fprintf(io, "  JUMP @exists.%i\n", next);
	fprintf(io, "create.%i:\n", next);
	fprintf(io, "  CALL &FS.MKDIR\n");
	fprintf(io, "exists.%i:\n", next);

	if (ENFORCED(r, RES_DIR_UID) || ENFORCED(r, RES_DIR_GID)) {
		fprintf(io, "COPY %%A %%F\n");
		fprintf(io, "SET %%D 0\n");
		fprintf(io, "SET %%E 0\n");

		if (ENFORCED(r, RES_DIR_UID)) {
			fprintf(io, "SET %%A 1\n");
			fprintf(io, "SET %%B \"%s\"\n", r->owner);
			fprintf(io, "CALL &USER.FIND\n");
			fprintf(io, "OK? @found.user.%i\n", next);
			fprintf(io, "  COPY %%B %%A\n");
			fprintf(io, "  PRINT \"Unable to find user '%%s'\\n\"\n");
			fprintf(io, "  JUMP @userfind.done.%i\n", next);
			fprintf(io, "found.user.%i:\n", next);
			fprintf(io, "CALL &USER.GET_UID\n");
			fprintf(io, "COPY %%R %%D\n");
			fprintf(io, "userfind.done.%i:\n", next);
		}

		if (ENFORCED(r, RES_DIR_GID)) {
			fprintf(io, "SET %%A 1\n");
			fprintf(io, "SET %%B \"%s\"\n", r->group);
			fprintf(io, "CALL &GROUP.FIND\n");
			fprintf(io, "OK? @found.group.%i\n", next);
			fprintf(io, "  COPY %%B %%A\n");
			fprintf(io, "  PRINT \"Unable to find group '%%s'\\n\"\n");
			fprintf(io, "  JUMP @groupfind.done.%i\n", next);
			fprintf(io, "found.group.%i:\n", next);
			fprintf(io, "CALL &GROUP.GET_GID\n");
			fprintf(io, "COPY %%R %%E\n");
			fprintf(io, "groupfind.done.%i:\n", next);
		}
		fprintf(io, "COPY %%F %%A\n");
		fprintf(io, "COPY %%D %%B\n");
		fprintf(io, "COPY %%E %%C\n");
		fprintf(io, "CALL &FS.CHOWN\n");
	}

	if (ENFORCED(r, RES_DIR_MODE)) {
		fprintf(io, "SET %%B 0%o\n", r->mode);
		fprintf(io, "CALL &FS.CHMOD\n");
	}

	return 0;
}

FILE * res_dir_content(const void *res, cw_hash_t *facts) { return NULL; }

int res_dir_notify(void *res, const struct resource *dep) { return 0; }

/********************************************************************/

void* res_exec_new(const char *key)
{
	return res_exec_clone(NULL, key);
}

void* res_exec_clone(const void *res, const char *key)
{
	const struct res_exec *orig = (const struct res_exec*)(res);
	struct res_exec *re = cw_alloc(sizeof(struct res_exec));

	re->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

	re->user  = RES_DEFAULT_STR(orig, user,  NULL);
	re->group = RES_DEFAULT_STR(orig, group, NULL);

	re->uid   = RES_DEFAULT(orig, uid, 0);
	re->gid   = RES_DEFAULT(orig, gid, 0);

	re->command = RES_DEFAULT_STR(orig, command, NULL);
	re->test    = RES_DEFAULT_STR(orig, test,    NULL);

	re->notified = 0;

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
		free(re->command);
		free(re->test);
		free(re->user);
		free(re->group);
		free(re->key);
	}
	free(re);
}

char *res_exec_key(const void *res)
{
	const struct res_exec *re = (const struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE

	return cw_string("exec:%s", re->key);
}

int res_exec_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_exec *re = (const struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE

	_hash_attr(attrs, "command", cw_strdup(re->command));
	_hash_attr(attrs, "test",    cw_strdup(re->test));
	_hash_attr(attrs, "user", ENFORCED(re, RES_EXEC_UID) ? strdup(re->user) : NULL);
	_hash_attr(attrs, "group", ENFORCED(re, RES_EXEC_GID) ? strdup(re->group) : NULL);
	_hash_attr(attrs, "ondemand", strdup(ENFORCED(re, RES_EXEC_ONDEMAND) ? "yes" : "no"));
	return 0;
}

int res_exec_norm(void *res, struct policy *pol, cw_hash_t *facts)
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
		free(re->user);
		re->user = strdup(value);
		ENFORCE(re, RES_EXEC_UID);

	} else if (strcmp(name, "group") == 0) {
		free(re->group);
		re->group = strdup(value);
		ENFORCE(re, RES_EXEC_GID);

	} else if (strcmp(name, "command") == 0) {
		free(re->command);
		re->command = strdup(value);

	} else if (strcmp(name, "test") == 0) {
		free(re->test);
		re->test = strdup(value);
		ENFORCE(re, RES_EXEC_TEST);

	} else if (strcmp(name, "ondemand") == 0) {
		if (strcmp(value, "no") == 0) {
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
		test_value = cw_string("%s", re->command);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_exec_gencode(const void *res, FILE *io, unsigned int next)
{
	return 0;
}

#if 0
static int _res_exec_run(const char *command, struct res_exec *re)
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
		if (ENFORCED(re, RES_EXEC_UID)) {
			if (setuid(re->uid) != 0) {
				cw_log(LOG_WARNING, "res_exec child could not switch to user ID %u to run `%s'", re->uid, command);
			} else {
				cw_log(LOG_DEBUG, "res_exec child set UID to %u", re->uid);
			}
		}

		if (ENFORCED(re, RES_EXEC_GID)) {
			if (setgid(re->gid) != 0) {
				cw_log(LOG_WARNING, "res_exec child could not switch to group ID %u to run `%s'", re->gid, command);
			} else {
				cw_log(LOG_DEBUG, "res_exec child set GID to %u", re->gid);
			}
		}

		execl("/bin/sh", "sh", "-c", command, (char*)NULL);
		exit(42); /* Oops... exec failed */

	default: /* parent */
		if (ENFORCED(re, RES_EXEC_UID) && ENFORCED(re, RES_EXEC_GID)) {
			cw_log(LOG_DEBUG, "res_exec: Running `%s' as %s:%s (%d:%d) in sub-process %u",
			      command, re->user, re->group, re->uid, re->gid, pid);
		} else if (ENFORCED(re, RES_EXEC_UID)) {
			cw_log(LOG_DEBUG, "res_exec: Running `%s' as user %s (%d) in sub-process %u",
			      command, re->user, re->uid, pid);
		} else if (ENFORCED(re, RES_EXEC_GID)) {
			cw_log(LOG_DEBUG, "res_exec: Running `%s' as group %s (%d) in sub-process %u",
			      command, re->group, re->gid, pid);
		} else {
			cw_log(LOG_DEBUG, "res_exec: Running `%s' in sub-process %u",
			      command, pid);
		}
	}

	waitpid(pid, &proc_stat, 0);
	if (!WIFEXITED(proc_stat)) {
		cw_log(LOG_DEBUG, "res_exec[%u]: terminated abnormally", pid);
		return -1;
	}

	cw_log(LOG_DEBUG, "res_exec[%u]: sub-process exited %u", pid, WEXITSTATUS(proc_stat));
	return WEXITSTATUS(proc_stat);
}

int res_exec_stat(void *res, const struct resource_env *env)
{
	struct res_exec *re = (struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE
	assert(re->command); // LCOV_EXCL_LINE

	if (re->user) {
		assert(env);            // LCOV_EXCL_LINE
		assert(env->user_pwdb); // LCOV_EXCL_LINE
		pwdb_lookup_uid(env->user_pwdb, re->user, &re->uid);
	}
	if (re->group) {
		assert(env);             // LCOV_EXCL_LINE
		assert(env->group_grdb); // LCOV_EXCL_LINE
		grdb_lookup_gid(env->group_grdb, re->group, &re->gid);
	}

	if (ENFORCED(re, RES_EXEC_TEST)) {
		if (_res_exec_run(re->test, re) == 0) {
			ENFORCE(re, RES_EXEC_NEEDSRUN);
		} else {
			UNENFORCE(re, RES_EXEC_NEEDSRUN);
		}
	} else if (!ENFORCED(re, RES_EXEC_ONDEMAND)) {
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
		action = cw_string("execute command");
		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (_res_exec_run(re->command, re) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}
	}

	return report;
}
#endif

FILE * res_exec_content(const void *res, cw_hash_t *facts) { return NULL; }

int res_exec_notify(void *res, const struct resource *dep)
{
	struct res_exec *re = (struct res_exec*)(res);
	assert(re); // LCOV_EXCL_LINE

	re->notified = 1;

	return 0;
}
