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

#include <fcntl.h>
#include <libgen.h>
#include <sys/wait.h>
#include <time.h>

#define ENFORCE(r,f)   (r)->enforced  |=  (f)
#define UNENFORCE(r,f) (r)->enforced  &= ~(f)

static int _group_update(struct stringlist*, struct stringlist*, const char*);
static int _setup_path_deps(const char *key, const char *path, struct policy *pol);
static void _hash_attr(cw_hash_t *attrs, const char *key, void *val);

#define RES_DEFAULT(orig,field,dflt) ((orig) ? (orig)->field : (dflt))
#define RES_DEFAULT_STR(orig,field,dflt) cw_strdup(RES_DEFAULT((orig),field,(dflt)))

/*****************************************************************/

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

int res_user_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_user *r = (struct res_user*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "TOPIC \"user(%s)\"\n", r->key);
	fprintf(io, "CALL &USERDB.OPEN\n");
	fprintf(io, "OK? @start.%u\n", next);
	fprintf(io, "  ERROR \"Failed to open the user database\"\n");
	fprintf(io, "  HALT\n");
	fprintf(io, "start.%u:\n", next);
	fprintf(io, "SET %%A 1\n");
	fprintf(io, "SET %%B \"%s\"\n", r->name);
	fprintf(io, "CALL &USER.FIND\n");

	if (ENFORCED(r, RES_USER_ABSENT)) {
		fprintf(io, "NOTOK? @next.%u\n", next);
		fprintf(io, "  CALL &USER.REMOVE\n");
	} else {
		fprintf(io, "OK? @check.ids.%u\n", next);
		if (ENFORCED(r, RES_USER_GID)) {
			fprintf(io, "  SET %%C %i\n", r->gid);
		} else {
			fprintf(io, "  SET %%A 1\n");
			fprintf(io, "  SET %%B \"%s\"\n", r->name);
			fprintf(io, "  CALL &GROUP.FIND\n");
			fprintf(io, "  OK? @group.found.%u\n", next);
			fprintf(io, "    COPY %%B %%A\n");
			fprintf(io, "    ERROR \"Failed to find group '%%s'\"\n");
			fprintf(io, "    JUMP @next.%u\n", next);
			fprintf(io, "  group.found.%u:\n", next);
			fprintf(io, "    CALL &GROUP.GET_GID\n");
			fprintf(io, "    COPY %%R %%C\n");
			fprintf(io, "group.done.%u:\n", next);
		}
		if (ENFORCED(r, RES_USER_UID)) {
			fprintf(io, "  SET %%B %i\n", r->uid);
		} else {
			fprintf(io, "  CALL &USER.NEXT_UID\n");
			fprintf(io, "  COPY %%R %%B\n");
		}
		fprintf(io, "  SET %%A \"%s\"\n", r->name);
		fprintf(io, "  CALL &USER.CREATE\n");
		fprintf(io, "  SET %%B \"x\"\n");
		fprintf(io, "  CALL &USER.SET_PASSWD\n");
		if (r->passwd && !ENFORCED(r, RES_USER_PASSWD)) {
			fprintf(io, "  SET %%B \"%s\"\n", r->passwd);
		} else {
			fprintf(io, "  SET %%B \"*\"\n");
		}
		fprintf(io, "  CALL &USER.SET_PWHASH\n");
		if (ENFORCED(r, RES_USER_MKHOME)) {
			fprintf(io, "  FLAG 1 :mkhome.%u\n", serial);
		}
		fprintf(io, "  JUMP @exists.%u\n", next);
		fprintf(io, "check.ids.%u:\n", next);
		if (ENFORCED(r, RES_USER_UID)) {
			fprintf(io, "SET %%B %i\n", r->uid);
			fprintf(io, "CALL &USER.SET_UID\n");
		}
		if (ENFORCED(r, RES_USER_GID)) {
			fprintf(io, "SET %%B %i\n", r->gid);
			fprintf(io, "CALL &USER.SET_GID\n");
		}
		fprintf(io, "exists.%u:\n", next);

		if (ENFORCED(r, RES_USER_PASSWD)) {
			fprintf(io, "SET %%B \"x\"\n");
			fprintf(io, "CALL &USER.SET_PASSWD\n");
			fprintf(io, "SET %%B \"%s\"\n", r->passwd);
			fprintf(io, "CALL &USER.SET_PWHASH\n");
		}

		if (ENFORCED(r, RES_USER_DIR)) {
			fprintf(io, "SET %%B \"%s\"\n", r->dir);
			fprintf(io, "CALL &USER.SET_HOME\n");
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

		if (ENFORCED(r, RES_USER_MKHOME)) {
			fprintf(io, "!FLAGGED? :mkhome.%u @home.exists.%u\n", serial, next);
			fprintf(io, "  CALL &USER.GET_UID\n");
			fprintf(io, "  COPY %%R %%B\n");
			fprintf(io, "  CALL &USER.GET_GID\n");
			fprintf(io, "  COPY %%R %%C\n");
			fprintf(io, "  CALL &USER.GET_HOME\n");
			fprintf(io, "  COPY %%R %%A\n");
			fprintf(io, "  CALL &FS.EXISTS?\n");
			fprintf(io, "  OK? @home.exists.%u\n", next);
			fprintf(io, "    CALL &FS.MKDIR\n");
			fprintf(io, "    CALL &FS.CHOWN\n");
			fprintf(io, "    SET %%D 0%o\n", 0700);
			fprintf(io, "    CALL &FS.CHMOD\n");
			if (r->skel) {
				fprintf(io, "    COPY %%C %%D\n");
				fprintf(io, "    COPY %%B %%C\n");
				fprintf(io, "    COPY %%A %%B\n");
				fprintf(io, "    SET %%A \"%s\"\n", r->skel);
				fprintf(io, "    CALL &FS.COPY_R\n");
			}
			fprintf(io, "home.exists.%u:\n", next);
		}
	}
	fprintf(io, "!FLAGGED? :changed @next.%u\n", next);
	fprintf(io, "  CALL &USERDB.SAVE\n");

return 0;
}

FILE * res_user_content(const void *res, cw_hash_t *facts) { return NULL; }


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
		free(rf->verify);
		free(rf->tmpfile);
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

int res_file_attrs(const void *res, cw_hash_t *attrs)
{
	const struct res_file *rf = (const struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

	_hash_attr(attrs, "path", strdup(rf->path));
	_hash_attr(attrs, "present", strdup(ENFORCED(rf, RES_FILE_ABSENT) ? "no" : "yes"));

	_hash_attr(attrs, "owner", ENFORCED(rf, RES_FILE_UID) ? strdup(rf->owner) : NULL);
	_hash_attr(attrs, "group", ENFORCED(rf, RES_FILE_GID) ? strdup(rf->group) : NULL);
	_hash_attr(attrs, "mode", ENFORCED(rf, RES_FILE_MODE) ? cw_string("%04o", rf->mode) : NULL);

	if (rf->verify) {
		_hash_attr(attrs, "verify", strdup(rf->verify));
		_hash_attr(attrs, "expect", cw_string("%i", rf->expectrc));
		if (rf->tmpfile)
			_hash_attr(attrs, "tmpfile", strdup(rf->tmpfile));
	}

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

	/* source and template attributes can have embedded '$path' */
	cw_hash_t *vars = cw_alloc(sizeof(cw_hash_t));
	cw_hash_set(vars, "path", rf->path);

	if (rf->source) {
		char *x = cw_interpolate(rf->source, vars);
		free(rf->source);
		rf->source = x;
	}
	if (rf->template) {
		char *x = cw_interpolate(rf->template, vars);
		free(rf->template);
		rf->template = x;
	}
	cw_hash_done(vars, 0);
	free(vars);

	/* derive tmpfile from path */
	if (!rf->tmpfile && rf->verify) {
		rf->tmpfile = cw_alloc(strlen(rf->path) + 7);
		char *slash = strrchr(rf->path, '/');
		if (!slash)
			slash = rf->path;

		int dir_len  = slash - rf->path + 1;
		int name_len = strlen(rf->path) - dir_len;

		/* everything up to the last dir component */
		memcpy(rf->tmpfile, rf->path, dir_len);
		rf->tmpfile[dir_len] = '.';
		memcpy(rf->tmpfile + dir_len + 1, rf->path + dir_len, name_len);
		memcpy(rf->tmpfile + dir_len + 1 + name_len, ".cogd", 5);

		cw_hash_t *vars = cw_alloc(sizeof(cw_hash_t));
		res_file_attrs(rf, vars);
		char *cmd = cw_interpolate(rf->verify, vars);
		cw_hash_done(vars, 1);
		free(vars);

		free(rf->verify);
		rf->verify = cmd;
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

	} else if (strcmp(name, "verify") == 0) {
		free(rf->verify);
		rf->verify = strdup(value);

	} else if (strcmp(name, "expect") == 0) {
		rf->expectrc = atoi(value);

	} else if (strcmp(name, "tmpfile") == 0) {
		free(rf->tmpfile);
		rf->tmpfile = strdup(value);

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

int res_file_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_file *r = (struct res_file*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "TOPIC \"file(%s)\"\n", r->key);
	fprintf(io, "SET %%A \"%s\"\n", r->path);
	fprintf(io, "CALL &FS.EXISTS?\n");
	if (ENFORCED(r, RES_FILE_ABSENT)) {
		fprintf(io, "NOTOK? @next.%u\n", next);
		fprintf(io, "CALL &FS.IS_FILE?\n");
		fprintf(io, "OK? @isfile.%u\n", next);
		fprintf(io, "  ERROR \"%%s exists, but is not a regular file\"\n");
		fprintf(io, "  JUMP @next.%u\n", next);
		fprintf(io, "isfile.%u:\n", next);
		fprintf(io, "CALL &FS.UNLINK\n");
		fprintf(io, "JUMP @next.%u\n", next);
		return 0;
	}
	fprintf(io, "OK? @exists.%u\n", next);
	fprintf(io, "  CALL &FS.MKFILE\n");
	fprintf(io, "  OK? @exists.%u\n", next);
	fprintf(io, "  ERROR \"Failed to create new file '%%s'\"\n");
	fprintf(io, "  JUMP @next.%u\n", next);
	fprintf(io, "exists.%u:\n",  next);
	fprintf(io, "CALL &FS.IS_FILE?\n");
	fprintf(io, "OK? @isfile.%u\n", next);
	fprintf(io, "  ERROR \"%%s exists, but is not a regular file\"\n");
	fprintf(io, "  JUMP @next.%u\n", next);
	fprintf(io, "isfile.%u:\n", next);

	if (ENFORCED(r, RES_FILE_UID) || ENFORCED(r, RES_FILE_GID)) {
		fprintf(io, "CALL &USERDB.OPEN\n");
		fprintf(io, "OK? @start.%u\n", next);
		fprintf(io, "  ERROR \"Failed to open the user database\"\n");
		fprintf(io, "  HALT\n");
		fprintf(io, "start.%u:\n", next);
		fprintf(io, "COPY %%A %%F\n");
		fprintf(io, "SET %%D 0\n");
		fprintf(io, "SET %%E 0\n");

		if (ENFORCED(r, RES_FILE_UID)) {
			fprintf(io, "SET %%A 1\n");
			fprintf(io, "SET %%B \"%s\"\n", r->owner);
			fprintf(io, "CALL &USER.FIND\n");
			fprintf(io, "OK? @found.user.%u\n", next);
			fprintf(io, "  COPY %%B %%A\n");
			fprintf(io, "  ERROR \"Failed to find user '%%s'\"\n");
			fprintf(io, "  JUMP @next.%u\n", next);
			fprintf(io, "found.user.%u:\n", next);
			fprintf(io, "CALL &USER.GET_UID\n");
			fprintf(io, "COPY %%R %%D\n");
		}

		if (ENFORCED(r, RES_FILE_GID)) {
			fprintf(io, "SET %%A 1\n");
			fprintf(io, "SET %%B \"%s\"\n", r->group);
			fprintf(io, "CALL &GROUP.FIND\n");
			fprintf(io, "OK? @found.group.%u\n", next);
			fprintf(io, "  COPY %%B %%A\n");
			fprintf(io, "  ERROR \"Failed to find group '%%s'\"\n");
			fprintf(io, "  JUMP @next.%u\n", next);
			fprintf(io, "found.group.%u:\n", next);
			fprintf(io, "CALL &GROUP.GET_GID\n");
			fprintf(io, "COPY %%R %%E\n");
		}
		fprintf(io, "CALL &USERDB.CLOSE\n");
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
		fprintf(io, "OK? @localok.%u\n", next);
		fprintf(io, "  ERROR \"Failed to calculate SHA1 for local copy of '%%s'\"\n");
		fprintf(io, "  JUMP @sha1.done.%u\n", next);
		fprintf(io, "localok.%u:\n", next);
		fprintf(io, "COPY %%S2 %%T1\n");
		fprintf(io, "COPY %%A %%F\n");
		fprintf(io, "SET %%A \"file:%s\"\n", r->key);
		if (r->source) {
			fprintf(io, ";; source = %s\n", r->source);
		} else if (r->template) {
			fprintf(io, ";; template = %s\n", r->template);
		}
		fprintf(io, "CALL &SERVER.SHA1\n");
		fprintf(io, "COPY %%F %%A\n");
		fprintf(io, "OK? @remoteok.%u\n", next);
		fprintf(io, "  ERROR \"Failed to retrieve SHA1 of expected contents\"\n");
		fprintf(io, "  JUMP @sha1.done.%u\n", next);
		fprintf(io, "remoteok.%u:\n", next);
		fprintf(io, "COPY %%S2 %%T2\n");
		fprintf(io, "CMP? @sha1.done.%u\n", next);
		fprintf(io, "  COPY %%T1 %%A\n");
		fprintf(io, "  COPY %%T2 %%B\n");
		fprintf(io, "  LOG NOTICE \"Updating local content (%%s) from remote copy (%%s)\"\n");
		fprintf(io, "  SET %%A \"%s\"\n", r->verify ? r->tmpfile : r->path);
		fprintf(io, "  CALL &SERVER.WRITEFILE\n");
		if (r->verify) {
			fprintf(io, "  OK? @tmpfile.done.%u\n", next);
			fprintf(io, "    ERROR \"Failed to update local file contents\"\n");
			fprintf(io, "    JUMP @sha1.done.%u\n", next);
			fprintf(io, "tmpfile.done.%u:\n", next);
			fprintf(io, "  SET %%A \"%s\"\n", r->verify);
			fprintf(io, "  SET %%B 0\n");
			fprintf(io, "  SET %%C 0\n");
			fprintf(io, "  CALL &EXEC.CHECK\n");
			fprintf(io, "  COPY %%R %%T1\n");
			fprintf(io, "  SET %%T2 %i\n", r->expectrc);
			fprintf(io, "  EQ? @rename.%u\n", next);
			fprintf(io, "    COPY %%R %%B\n");
			fprintf(io, "    COPY %%T2 %%C\n");
			fprintf(io, "    ERROR \"Pre-change verification check `%%s` failed; returned %%i (not %%i)\"\n");
			fprintf(io, "    JUMP @sha1.done.%u\n", next);
			fprintf(io, "rename.%u:\n", next);
			fprintf(io, "  SET %%A \"%s\"\n", r->tmpfile);
			fprintf(io, "  SET %%B \"%s\"\n", r->path);
			fprintf(io, "  CALL &FS.RENAME\n");
			fprintf(io, "  OK? @sha1.done.%u\n", next);
			fprintf(io, "    ERROR \"Failed to update local file contents\"\n");
			fprintf(io, "    CALL &FS.UNLINK\n");
		} else {
			fprintf(io, "  OK? @sha1.done.%u\n", next);
			fprintf(io, "    ERROR \"Failed to update local file contents\"\n");
		}
		fprintf(io, "sha1.done.%u:\n", next);
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

/*****************************************************************/

void* res_symlink_new(const char *key)
{
	return res_symlink_clone(NULL, key);
}

void* res_symlink_clone(const void *res, const char *key)
{
	const struct res_symlink *orig = (const struct res_symlink*)(res);
	struct res_symlink *r = cw_alloc(sizeof(struct res_symlink));

	r->path   = RES_DEFAULT_STR(orig, path,   NULL);
	r->target = RES_DEFAULT_STR(orig, target, "/dev/null");

	r->key = NULL;
	if (key) {
		r->key = strdup(key);
		res_symlink_set(r, "path", key);
	}

	return r;
}

void res_symlink_free(void *res)
{
	struct res_symlink *r = (struct res_symlink*)(res);
	if (r) {
		free(r->path);
		free(r->target);
		free(r->key);
	}

	free(r);
}

char* res_symlink_key(const void *res)
{
	struct res_symlink *r = (struct res_symlink*)(res);
	assert(r); // LCOV_EXCL_LINE

	return cw_string("symlink:%s", r->key);
}

int res_symlink_attrs(const void *res, cw_hash_t *attrs)
{
	struct res_symlink *r = (struct res_symlink*)(res);
	assert(r); // LCOV_EXCL_LINE

	_hash_attr(attrs, "present", strdup(r->absent ? "no" : "yes"));
	_hash_attr(attrs, "path",    strdup(r->path));
	_hash_attr(attrs, "target",  strdup(r->target));
	return 0;
}

int res_symlink_norm(void *res, struct policy *pol, cw_hash_t *facts)
{
	struct res_symlink *r = (struct res_symlink*)(res);
	assert(r); // LCOV_EXCL_LINE

	char *key = res_symlink_key(r);

	/* files depend on res_dir resources between them and / */
	if (_setup_path_deps(key, r->path, pol) != 0) {
		free(key);
		return -1;
	}

	free(key);
	return 0;
}

int res_symlink_set(void *res, const char *name, const char *value)
{
	struct res_symlink *r = (struct res_symlink*)(res);
	assert(r); // LCOV_EXCL_LINE

	if (strcmp(name, "path") == 0) {
		free(r->path);
		r->path = strdup(value);

	} else if (strcmp(name, "target") == 0) {
		free(r->target);
		r->target = strdup(value);

	} else if (strcmp(name, "present") == 0) {
		r->absent = strcmp(value, "no") == 0 ? 1 : 0;

	} else {
		/* unknown attribute. */
		return -1;
	}

	return 0;
}

int res_symlink_match(const void *res, const char *name, const char *value)
{
	const struct res_symlink *r = (struct res_symlink*)(res);
	assert(r); // LCOV_EXCL_LINE

	char *test_value;
	int rc;

	if (strcmp(name, "path") == 0) {
		test_value = cw_string("%s", r->path);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_symlink_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_symlink *r = (struct res_symlink*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "TOPIC \"symlink(%s)\"\n", r->key);
	fprintf(io, "SET %%A \"%s\"\n", r->path);
	fprintf(io, "SET %%B \"%s\"\n", r->target);

	fprintf(io, "CALL &FS.EXISTS?\n");
	if (r->absent) {
		fprintf(io, "NOTOK? @next.%u\n", next);
		fprintf(io, "CALL &FS.IS_SYMLINK?\n");
		fprintf(io, "OK? @islink.%u\n", next);
		fprintf(io, "  ERROR \"%%s exists, but is not a symlink\\n\"\n");
		fprintf(io, "  JUMP @next.%u\n", next);
		fprintf(io, "islink.%u:\n", next);
		fprintf(io, "CALL &FS.UNLINK\n");
		fprintf(io, "JUMP @next.%u\n", next);
		return 0;
	}
	fprintf(io, "NOTOK? @create.%u\n", next);
	fprintf(io, "  CALL &FS.IS_SYMLINK?\n");
	fprintf(io, "  OK? @islink.%u\n", next);
	fprintf(io, "    ERROR \"%%s exists, but is not a symlink\\n\"\n");
	fprintf(io, "    JUMP @next.%u\n", next);
	fprintf(io, "  islink.%u:\n", next);
	fprintf(io, "  CALL &FS.UNLINK\n");
	fprintf(io, "CALL &FS.EXISTS?\n");
	fprintf(io, "NOTOK? @create.%u\n", next);
	fprintf(io, "  ERROR \"Failed to unlink %%s\\n\"\n");
	fprintf(io, "  JUMP @next.%u\n", next);
	fprintf(io, "create.%u:\n", next);
	fprintf(io, "CALL &FS.SYMLINK\n");
	fprintf(io, "OK? @next.%u\n", next);
	fprintf(io, "  ERROR \"Failed to symlink %%s -> %%s\\n\"\n");
	return 0;
}

FILE * res_symlink_content(const void *res, cw_hash_t *facts) { return NULL; }


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

int res_group_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_group *r = (struct res_group*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "TOPIC \"group(%s)\"\n", r->key);
	fprintf(io, "CALL &USERDB.OPEN\n");
	fprintf(io, "OK? @start.%u\n", next);
	fprintf(io, "  ERROR \"Failed to open the user database\"\n");
	fprintf(io, "  HALT\n");
	fprintf(io, "start.%u:\n", next);
	fprintf(io, "SET %%A 1\n");
	fprintf(io, "SET %%B \"%s\"\n", r->name);
	fprintf(io, "CALL &GROUP.FIND\n");

	if (ENFORCED(r, RES_GROUP_ABSENT)) {
		fprintf(io, "CALL &GROUP.REMOVE\n");
	} else {
		fprintf(io, "OK? @found.%u\n", next);
		fprintf(io, "  COPY %%B %%A\n");

		if (ENFORCED(r, RES_GROUP_GID)) {
			fprintf(io, "  SET %%B %i\n", r->gid);
		} else {
			fprintf(io, "  CALL &GROUP.NEXT_GID\n");
			fprintf(io, "  COPY %%R %%B\n");
		}

		fprintf(io, "  CALL &GROUP.CREATE\n");
		fprintf(io, "  JUMP @update.%u\n", next);
		fprintf(io, "found.%u:\n", next);

		if (ENFORCED(r, RES_GROUP_GID)) {
			fprintf(io, "  SET %%B %i\n", r->gid);
			fprintf(io, "  CALL &GROUP.SET_GID\n");
		}

		fprintf(io, "update.%u:\n", next);
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
				fprintf(io, "OK? @has-member.%s.%u\n", *name, next);
				fprintf(io, "  CALL &GROUP.ADD_MEMBER\n");
				fprintf(io, "  FLAG 1 :changed\n");
				fprintf(io, "has-member.%s.%u:\n", *name, next);
			}
			for (name = r->mem_rm->strings; *name; name++) {
				fprintf(io, "SET %%A \"%s\"\n", *name);
				fprintf(io, "CALL &GROUP.HAS_MEMBER?\n");
				fprintf(io, "NOTOK? @no-member.%s.%u\n", *name, next);
				fprintf(io, "  CALL &GROUP.RM_MEMBER\n");
				fprintf(io, "  FLAG 1 :changed\n");
				fprintf(io, "no-member.%s.%u:\n", *name, next);
			}
		}

		if (ENFORCED(r, RES_GROUP_ADMINS)) {
			fprintf(io, ";; admins\n");
			char ** name;
			for (name = r->adm_add->strings; *name; name++) {
				fprintf(io, "SET %%A \"%s\"\n", *name);
				fprintf(io, "CALL &GROUP.HAS_ADMIN?\n");
				fprintf(io, "OK? @has-admin.%s.%u\n", *name, next);
				fprintf(io, "  CALL &GROUP.ADD_ADMIN\n");
				fprintf(io, "  FLAG 1 :changed\n");
				fprintf(io, "has-admin.%s.%u:\n", *name, next);
			}
			for (name = r->adm_rm->strings; *name; name++) {
				fprintf(io, "SET %%A \"%s\"\n", *name);
				fprintf(io, "CALL &GROUP.HAS_ADMIN?\n");
				fprintf(io, "NOTOK? @no-admin.%s.%u\n", *name, next);
				fprintf(io, "  CALL &GROUP.RM_ADMIN\n");
				fprintf(io, "  FLAG 1 :changed\n");
				fprintf(io, "no-admin.%s.%u:\n", *name, next);
			}
		}
	}
	fprintf(io, "!FLAGGED? :changed @next.%u\n", next);
	fprintf(io, "  CALL &USERDB.SAVE\n");

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
		rp->version = NULL;
		rp->latest = 0;
		if (strcmp(value, "latest") == 0) {
			rp->latest = 1;
		} else if (strcmp(value, "any") != 0) {
			rp->version = strdup(value);
		}

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

int res_package_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_package *r = (struct res_package*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "TOPIC \"package(%s)\"\n", r->key);
	fprintf(io, "SET %%A \"cw localsys pkg-version %s\"\n", r->name);
	fprintf(io, "CALL &EXEC.RUN1\n");
	if (ENFORCED(r, RES_PACKAGE_ABSENT)) {
		fprintf(io, "NOTOK? @next.%u\n", next);
		fprintf(io, "  LOG NOTICE \"uninstalling %s\"\n", r->name);
		fprintf(io, "  SET %%A \"cw localsys pkg-remove %s\"\n", r->name);
		fprintf(io, "  CALL &EXEC.CHECK\n");
		fprintf(io, "  FLAG 1 :changed\n");
		return 0;
	}

	if (!r->version && !r->latest) {
		fprintf(io, "OK? @next.%u\n", next);
		fprintf(io, "  LOG NOTICE \"installing %s\"\n", r->name);
		fprintf(io, "  SET %%A \"cw localsys pkg-install %s latest\"\n", r->name);
		fprintf(io, "  CALL &EXEC.CHECK\n");
		fprintf(io, "  FLAG 1 :changed\n");
		return 0;
	}

	fprintf(io, "OK? @installed.%u\n", next);
	if (r->version) {
		fprintf(io, "  LOG NOTICE \"installing %s version %s\"\n", r->name, r->version);
	} else {
		fprintf(io, "  LOG NOTICE \"installing latest version of %s\"\n", r->name);
	}
	fprintf(io, "  SET %%A \"cw localsys pkg-install %s %s\"\n", r->name, r->version ? r->version : "latest");
	fprintf(io, "  CALL &EXEC.CHECK\n");
	fprintf(io, "  FLAG 1 :changed\n");
	fprintf(io, "  JUMP @next.%u\n", next);
	fprintf(io, "installed.%u:\n", next);
	fprintf(io, "COPY %%S2 %%T1\n");
	if (r->latest) {
		fprintf(io, "SET %%A \"cw localsys pkg-latest %s\"\n", r->name);
		fprintf(io, "CALL &EXEC.RUN1\n");
		fprintf(io, "OK? @got.latest.%u\n", next);
		fprintf(io, "  ERROR \"Failed to detect latest version of '%s'\"\n", r->name);
		fprintf(io, "  JUMP @next.%u\n", next);
		fprintf(io, "got.latest.%u:\n", next);
		fprintf(io, "COPY %%S2 %%T2\n");
	} else {
		fprintf(io, "SET %%T2 \"%s\"\n", r->version);
	}
	fprintf(io, "CALL &UTIL.VERCMP\n");
	fprintf(io, "OK? @next.%u\n", next);
	if (r->latest) {
		fprintf(io, "  LOG NOTICE \"upgrading to latest version of %s\"\n", r->name);
	} else {
		fprintf(io, "  LOG NOTICE \"upgrading to %s version %s\"\n", r->name, r->version);
	}
	fprintf(io, "  SET %%A \"cw localsys pkg-install %s %s\"\n", r->name, r->latest ? "latest" : r->version);
	fprintf(io, "  CALL &EXEC.CHECK\n");
	fprintf(io, "  FLAG 1 :changed\n");
	return 0;
}

FILE * res_package_content(const void *res, cw_hash_t *facts) { return NULL; }

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
	rs->notify    = RES_DEFAULT_STR(orig, notify,  NULL);

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
		free(rs->notify);

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
	_hash_attr(attrs, "notify", cw_strdup(rs->notify));
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

	} else if (strcmp(name, "notify") == 0) {
		free(rs->notify);
		rs->notify = strdup(value);

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

int res_service_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_service *r = (struct res_service*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "TOPIC \"service(%s)\"\n", r->key);
	if (ENFORCED(r, RES_SERVICE_ENABLED)) {
		fprintf(io, "SET %%A \"cw localsys svc-boot-status %s\"\n", r->service);
		fprintf(io, "CALL &EXEC.CHECK\n");
		fprintf(io, "OK? @enabled.%u\n", next);
		fprintf(io, "  LOG NOTICE \"enabling service %s to start at boot\"\n", r->service);
		fprintf(io, "  SET %%A \"cw localsys svc-enable %s\"\n", r->service);
		fprintf(io, "  CALL &EXEC.CHECK\n");
		fprintf(io, "enabled.%u:\n", next);

	} else if (ENFORCED(r, RES_SERVICE_DISABLED)) {
		fprintf(io, "SET %%A \"cw localsys svc-boot-status %s\"\n", r->service);
		fprintf(io, "CALL &EXEC.CHECK\n");
		fprintf(io, "NOTOK? @disabled.%u\n", next);
		fprintf(io, "  LOG NOTICE \"disabling service %s\"\n", r->service);
		fprintf(io, "  SET %%A \"cw localsys svc-disable %s\"\n", r->service);
		fprintf(io, "  CALL &EXEC.CHECK\n");
		fprintf(io, "disabled.%u:\n", next);
	}

	if (ENFORCED(r, RES_SERVICE_RUNNING)) {
		fprintf(io, "SET %%A \"cw localsys svc-run-status %s\"\n", r->service);
		fprintf(io, "CALL &EXEC.CHECK\n");
		fprintf(io, "OK? @running.%u\n", next);
		fprintf(io, "  LOG NOTICE \"starting service %s\"\n", r->service);
		fprintf(io, "  SET %%A \"cw localsys svc-init %s start\"\n", r->service);
		fprintf(io, "  CALL &EXEC.CHECK\n");
		fprintf(io, "  FLAG 1 :changed\n");
		fprintf(io, "  FLAG 0 :res%u\n", serial);
		fprintf(io, "running.%u:\n", next);

	} else if (ENFORCED(r, RES_SERVICE_STOPPED)) {
		fprintf(io, "SET %%A \"cw localsys svc-run-status %s\"\n", r->service);
		fprintf(io, "CALL &EXEC.CHECK\n");
		fprintf(io, "NOTOK? @stopped.%u\n", next);
		fprintf(io, "  LOG NOTICE \"stopping service %s\"\n", r->service);
		fprintf(io, "  SET %%A \"cw localsys svc-init %s stop\"\n", r->service);
		fprintf(io, "  CALL &EXEC.CHECK\n");
		fprintf(io, "  FLAG 1 :changed\n");
		fprintf(io, "  FLAG 0 :res%u\n", serial);
		fprintf(io, "stopped.%u:\n", next);
	}

	fprintf(io, "!FLAGGED? :res%u @next.%u\n", serial, next);

	if (ENFORCED(r, RES_SERVICE_RUNNING)) {
		fprintf(io, "  LOG NOTICE \"%sing service %s\"\n", r->notify ? r->notify : "restart", r->service);
		fprintf(io, "  SET %%A \"cw localsys svc-init %s %s\"\n",
				r->service, r->notify ? r->notify : "restart");
		fprintf(io, "  CALL &EXEC.CHECK\n");
		fprintf(io, "  FLAG 0 :res%u\n", serial);

	} else if (ENFORCED(r, RES_SERVICE_STOPPED)) {
		fprintf(io, "  LOG NOTICE \"stopping service %s\"\n", r->service);
		fprintf(io, "  SET %%A \"cw localsys svc-init %s stop\"\n", r->service);
		fprintf(io, "  CALL &EXEC.CHECK\n");
		fprintf(io, "  FLAG 0 :res%u\n", serial);
	}


	return 0;
}

FILE * res_service_content(const void *res, cw_hash_t *facts) { return NULL; }

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

int res_host_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_host *r = (struct res_host*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "TOPIC \"host(%s)\"\n", r->key);
	fprintf(io, "SET %%A \"/files/etc/hosts/*[ipaddr = \\\"%s\\\" and canonical = \\\"%s\\\"]\"\n", r->ip, r->hostname);
	fprintf(io, "CALL &AUGEAS.FIND\n");

	if (ENFORCED(r, RES_HOST_ABSENT)) {
		fprintf(io, "NOTOK? @not.found.%u\n", next);
		fprintf(io, "  COPY %%R %%A\n");
		fprintf(io, "  CALL &AUGEAS.REMOVE\n");
		fprintf(io, "not.found.%u:\n", next);

	} else {
		fprintf(io, "OK? @found.%u\n", next);
		fprintf(io, "  SET %%A \"/files/etc/hosts/%u/ipaddr\"\n", 99999+next);
		fprintf(io, "  SET %%B \"%s\"\n", r->ip);
		fprintf(io, "  CALL &AUGEAS.SET\n");
		fprintf(io, "  SET %%A \"/files/etc/hosts/%u/canonical\"\n", 99999+next);
		fprintf(io, "  SET %%B \"%s\"\n", r->hostname);
		fprintf(io, "  CALL &AUGEAS.SET\n");
		fprintf(io, "  JUMP @aliases.%u\n", next);
		fprintf(io, "found.%u:\n", next);
		fprintf(io, "  COPY %%S2 %%A\n");

		fprintf(io, "aliases.%u:\n", next);
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

int res_sysctl_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_sysctl *r = (struct res_sysctl*)(res);
	assert(r); // LCOV_EXCL_LINE

	char *p, *path = strdup(r->param);
	for (p = path; *p; p++)
		if (*p == '.') *p = '/';

	fprintf(io, "TOPIC \"sysctl(%s)\"\n", r->key);
	if (ENFORCED(r, RES_SYSCTL_VALUE)) {
		fprintf(io, "SET %%A \"/proc/sys/%s\"\n", path);
		fprintf(io, "CALL &FS.GET\n");
		fprintf(io, "COPY %%S2 %%T1\n");
		fprintf(io, "SET %%T2 \"%s\"\n", r->value);
		fprintf(io, "CMP? @done.%u\n", next);
		fprintf(io, "  COPY %%T2 %%B\n");
		fprintf(io, "  CALL &FS.PUT\n");
		fprintf(io, "done.%u:\n", next);

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

int res_dir_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_dir *r = (struct res_dir*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "TOPIC \"dir(%s)\"\n", r->key);
	fprintf(io, "SET %%A \"%s\"\n", r->path);

	fprintf(io, "CALL &FS.EXISTS?\n");
	if (ENFORCED(r, RES_DIR_ABSENT)) {
		fprintf(io, "NOTOK? @next.%u\n", next);
		fprintf(io, "CALL &FS.IS_DIR?\n");
		fprintf(io, "OK? @isdir.%u\n", next);
		fprintf(io, "  ERROR \"%%s exists, but is not a directory\"\n");
		fprintf(io, "  JUMP @next.%u\n", next);
		fprintf(io, "isdir.%u:\n", next);
		fprintf(io, "CALL &FS.RMDIR\n");
		fprintf(io, "JUMP @next.%u\n", next);
		return 0;
	}
	fprintf(io, "OK? @exists.%u\n", next);
	fprintf(io, "  CALL &FS.MKDIR\n");
	fprintf(io, "  OK? @exists.%u\n", next);
	fprintf(io, "  ERROR \"Failed to create new directory '%%s'\"\n");
	fprintf(io, "  JUMP @next.%u\n", next);
	fprintf(io, "exists.%u:\n", next);
	fprintf(io, "CALL &FS.IS_DIR?\n");
	fprintf(io, "OK? @isdir.%u\n", next);
	fprintf(io, "  ERROR \"%%s exists, but is not a directory\"\n");
	fprintf(io, "  JUMP @next.%u\n", next);
	fprintf(io, "isdir.%u:\n", next);

	if (ENFORCED(r, RES_DIR_UID) || ENFORCED(r, RES_DIR_GID)) {
		fprintf(io, "COPY %%A %%F\n");
		fprintf(io, "SET %%D 0\n");
		fprintf(io, "SET %%E 0\n");

		if (ENFORCED(r, RES_DIR_UID) || ENFORCED(r, RES_DIR_GID)) {
			fprintf(io, "CALL &USERDB.OPEN\n");
			fprintf(io, "OK? @owner.lookup.%u\n", next);
			fprintf(io, "  ERROR \"Failed to open the user database\"\n");
			fprintf(io, "  HALT\n");
			fprintf(io, "owner.lookup.%u:\n", next);
			if (ENFORCED(r, RES_DIR_UID)) {
				fprintf(io, "SET %%A 1\n");
				fprintf(io, "SET %%B \"%s\"\n", r->owner);
				fprintf(io, "CALL &USER.FIND\n");
				fprintf(io, "OK? @found.user.%u\n", next);
				fprintf(io, "  COPY %%B %%A\n");
				fprintf(io, "  ERROR \"Failed to find user '%%s'\"\n");
				fprintf(io, "  JUMP @next.%u\n", next);
				fprintf(io, "found.user.%u:\n", next);
				fprintf(io, "CALL &USER.GET_UID\n");
				fprintf(io, "COPY %%R %%D\n");
			}

			if (ENFORCED(r, RES_DIR_GID)) {
				fprintf(io, "SET %%A 1\n");
				fprintf(io, "SET %%B \"%s\"\n", r->group);
				fprintf(io, "CALL &GROUP.FIND\n");
				fprintf(io, "OK? @found.group.%u\n", next);
				fprintf(io, "  COPY %%B %%A\n");
				fprintf(io, "  ERROR \"Failed to find group '%%s'\"\n");
				fprintf(io, "  JUMP @next.%u\n", next);
				fprintf(io, "found.group.%u:\n", next);
				fprintf(io, "CALL &GROUP.GET_GID\n");
				fprintf(io, "COPY %%R %%E\n");
			}

			fprintf(io, "CALL &USERDB.CLOSE\n");
		}
		fprintf(io, "COPY %%F %%A\n");
		fprintf(io, "COPY %%D %%B\n");
		fprintf(io, "COPY %%E %%C\n");
		fprintf(io, "CALL &FS.CHOWN\n");
	}

	if (ENFORCED(r, RES_DIR_MODE)) {
		fprintf(io, "SET %%D 0%o\n", r->mode);
		fprintf(io, "CALL &FS.CHMOD\n");
	}

	return 0;
}

FILE * res_dir_content(const void *res, cw_hash_t *facts) { return NULL; }

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

	if (strcmp(name, "command") == 0) {
		test_value = cw_string("%s", re->command);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_exec_gencode(const void *res, FILE *io, unsigned int next, unsigned int serial)
{
	struct res_exec *r = (struct res_exec*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "TOPIC \"exec(%s)\"\n", r->key);
	if (ENFORCED(r, RES_EXEC_UID) || ENFORCED(r, RES_EXEC_GID)) {
		fprintf(io, "CALL &USERDB.OPEN\n");
		fprintf(io, "OK? @who.lookup.%u\n", next);
		fprintf(io, "  ERROR \"Failed to open the user database\"\n");
		fprintf(io, "  HALT\n");
		fprintf(io, "who.lookup.%u:\n", next);
		fprintf(io, "SET %%D 0\n");
		fprintf(io, "SET %%E 0\n");
		if (ENFORCED(r, RES_EXEC_UID)) {
			fprintf(io, "SET %%A 1\n");
			fprintf(io, "SET %%B \"%s\"\n", r->user);
			fprintf(io, "CALL &USER.FIND\n");
			fprintf(io, "OK? @user.found.%u\n", next);
			fprintf(io, "  ERROR \"Failed to find user '%s'\"\n", r->user);
			fprintf(io, "  HALT\n");
			fprintf(io, "user.found.%u:\n", next);
			fprintf(io, "CALL &USER.GET_UID\n");
			fprintf(io, "COPY %%R %%D\n");
		}
		if (ENFORCED(r, RES_EXEC_GID)) {
			fprintf(io, "SET %%A 1\n");
			fprintf(io, "SET %%B \"%s\"\n", r->group);
			fprintf(io, "CALL &GROUP.FIND\n");
			fprintf(io, "OK? @group.found.%u\n", next);
			fprintf(io, "  ERROR \"Failed to find group '%s'\"\n", r->group);
			fprintf(io, "  HALT\n");
			fprintf(io, "group.found.%u:\n", next);
			fprintf(io, "CALL &GROUP.GET_GID\n");
			fprintf(io, "COPY %%R %%E\n");
		}
		fprintf(io, "COPY %%D %%B\n");
		fprintf(io, "COPY %%E %%C\n");
		fprintf(io, "CALL &USERDB.CLOSE\n");
	} else {
		fprintf(io, "SET %%B 0\n");
		fprintf(io, "SET %%C 0\n");
	}
	if (ENFORCED(r, RES_EXEC_TEST)) {
		fprintf(io, "SET %%A \"%s\"\n", r->test);
		fprintf(io, "CALL &EXEC.CHECK\n");
		fprintf(io, "NOTOK? @next.%u\n", next);
	}
	if (ENFORCED(r, RES_EXEC_ONDEMAND)) {
		fprintf(io, "!FLAGGED? :res%u @next.%u\n", serial, next);
	}
	fprintf(io, "SET %%A \"%s\"\n", r->command);
	fprintf(io, "CALL &EXEC.CHECK\n");
	fprintf(io, "OK? @next.%u\n", next);
	fprintf(io, "  FLAG 1 :changed\n");
	return 0;
}

FILE * res_exec_content(const void *res, cw_hash_t *facts) { return NULL; }
