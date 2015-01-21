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

static int _group_update(strings_t*, strings_t*, const char*);
static int _setup_path_deps(const char *key, const char *path, struct policy *pol);
static void _hash_attr(hash_t *attrs, const char *key, void *val);

#define RES_DEFAULT(orig,field,dflt) ((orig) ? (orig)->field : (dflt))
#define RES_DEFAULT_STR(orig,field,dflt) cw_strdup(RES_DEFAULT((orig),field,(dflt)))

/*****************************************************************/

static int _group_update(strings_t *add, strings_t *rm, const char *user)
{
	/* put user in add */
	if (strings_search(add, user) != 0) {
		strings_add(add, user);
	}

	/* take user out of rm */
	if (strings_search(rm, user) == 0) {
		strings_remove(rm, user);
	}

	return 0;
}

static int _setup_path_deps(const char *key, const char *spath, struct policy *pol)
{
	path_t *p;
	struct dependency *dep;
	struct resource *dir;

	logger(LOG_DEBUG, "setup_path_deps: setting up for %s (%s)", spath, key);
	p = path_new(spath);
	if (!p) {
		return -1;
	}
	if (path_canon(p) != 0) { goto failed; }

	while (path_pop(p) != 0) {
		/* look for deps, and add them. */
		logger(LOG_DEBUG, "setup_path_deps: looking for %s", path(p));
		dir = policy_find_resource(pol, RES_DIR, "path", path(p));
		if (dir) {
			dep = dependency_new(key, dir->key);
			if (policy_add_dependency(pol, dep) != 0) {
				dependency_free(dep);
				goto failed;
			}
		} else {
			logger(LOG_DEBUG, "setup_path_deps: no res_dir defined for '%s'", path(p));
		}
	}

	path_free(p);
	return 0;

failed:
	logger(LOG_DEBUG, "setup_path_deps: unspecified failure");
	path_free(p);
	return -1;
}

static void _hash_attr(hash_t *attrs, const char *key, void *val)
{
	void *prev = hash_get(attrs, key);
	hash_set(attrs, key, val);
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
	struct res_user *ru = ru = vmalloc(sizeof(struct res_user));

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

	return string("user:%s", ru->key);
}

int res_user_attrs(const void *res, hash_t *attrs)
{
	const struct res_user *ru = (const struct res_user*)(res);
	assert(ru); // LCOV_EXCL_LINE

	_hash_attr(attrs, "username", strdup(ru->name));
	_hash_attr(attrs, "uid", ENFORCED(ru, RES_USER_UID) ? string("%u",ru->uid) : NULL);
	_hash_attr(attrs, "gid", ENFORCED(ru, RES_USER_GID) ? string("%u",ru->gid) : NULL);
	_hash_attr(attrs, "home", ENFORCED(ru, RES_USER_DIR) ? strdup(ru->dir) : NULL);
	_hash_attr(attrs, "present", strdup(ENFORCED(ru, RES_USER_ABSENT) ? "no" : "yes"));
	_hash_attr(attrs, "locked", ENFORCED(ru, RES_USER_LOCK) ? strdup(ru->lock ? "yes" : "no") : NULL);
	_hash_attr(attrs, "comment", ENFORCED(ru, RES_USER_GECOS) ? strdup(ru->gecos) : NULL);
	_hash_attr(attrs, "shell", ENFORCED(ru, RES_USER_SHELL) ? strdup(ru->shell) : NULL);
	_hash_attr(attrs, "password", ENFORCED(ru, RES_USER_PASSWD) ? strdup(ru->passwd) : NULL);
	_hash_attr(attrs, "pwmin", ENFORCED(ru, RES_USER_PWMIN) ? string("%u", ru->pwmin) : NULL);
	_hash_attr(attrs, "pwmax", ENFORCED(ru, RES_USER_PWMAX) ? string("%u", ru->pwmax) : NULL);
	_hash_attr(attrs, "pwwarn", ENFORCED(ru, RES_USER_PWWARN) ? string("%u", ru->pwwarn) : NULL);
	_hash_attr(attrs, "inact", ENFORCED(ru, RES_USER_INACT) ? string("%u", ru->inact) : NULL);
	_hash_attr(attrs, "expiration", ENFORCED(ru, RES_USER_EXPIRE) ? string("%u", ru->expire) : NULL);
	_hash_attr(attrs, "skeleton", ENFORCED(ru, RES_USER_MKHOME) ? strdup(ru->skel) : NULL);
	return 0;
}

int res_user_norm(void *res, struct policy *pol, hash_t *facts) { return 0; }

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
		test_value = string("%u", ru->uid);
	} else if (strcmp(name, "gid") == 0) {
		test_value = string("%u", ru->gid);
	} else if (strcmp(name, "username") == 0) {
		test_value = string("%s", ru->name);
	} else if (strcmp(name, "home") == 0) {
		test_value = string("%s", ru->dir);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_user_gencode(const void *res, FILE *io)
{
	struct res_user *r = (struct res_user*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "  call util.authdb.open\n"
	            "  set %%a \"%s\"\n", r->name);

	if (ENFORCED(r, RES_USER_ABSENT)) {
		fprintf(io, "  call res.user.absent\n"
		            "  call util.authdb.save\n");
		return 0;
	}

	char *s = string("/home/%s", r->name);
	fprintf(io, "  set %%b %u ; uid\n"
	            "  set %%c %u ; gid\n"
	            "  set %%d \"%s\" ; home\n"
	            "  set %%e \"%s\" ; shell\n"
	            "  set %%f \"%s\" ; password\n"
	            "  call res.user.present\n",
	            ENFORCED(r, RES_USER_UID)   ? r->uid   : 0xffffffff,
	            ENFORCED(r, RES_USER_GID)   ? r->gid   : 0xffffffff,
	            ENFORCED(r, RES_USER_DIR)   ? r->dir   : s,
	            ENFORCED(r, RES_USER_SHELL) ? r->shell : "",
	            r->passwd ? r->passwd : "*");

	if (ENFORCED(r, RES_USER_DIR))
		fprintf(io, "  ;;; home\n"
		            "  set %%b \"%s\"\n"
		            "  user.set \"home\" %%b\n"
		            "  jz +2\n"
		            "    error \"Failed to set %%[a]s' home directory to %%[b]s\"\n"
		            "    bail 1\n", r->dir);
	if (ENFORCED(r, RES_USER_GECOS))
		fprintf(io, "  ;;; comment\n"
		            "  set %%b \"%s\"\n"
		            "  user.set \"comment\" %%b\n"
		            "  jz +2\n"
		            "    error \"Failed to set %%[a]s' GECOS comment to %%[b]s\"\n"
		            "    bail 1\n", r->gecos);
	if (ENFORCED(r, RES_USER_SHELL))
		fprintf(io, "  ;;; login shell\n"
		            "  set %%b \"%s\"\n"
		            "  user.set \"shell\" %%b\n"
		            "  jz +2\n"
		            "    error \"Failed to set %%[a]s' login shell to %%[b]s\"\n"
		            "    bail 1\n", r->shell);
	if (ENFORCED(r, RES_USER_PWMIN))
		fprintf(io, "  ;;; minimum password age\n"
		            "  set %%b \"%li\"\n"
		            "  user.set \"pwmin\" %%b\n"
		            "  jz +2\n"
		            "    error \"Failed to set %%[a]s' minimum password age to %%[b]li\"\n"
		            "    bail 1\n", r->pwmin);
	if (ENFORCED(r, RES_USER_PWMAX))
		fprintf(io, "  ;;; maximum password age\n"
		            "  set %%b \"%li\"\n"
		            "  user.set \"pwmax\" %%b\n"
		            "  jz +2\n"
		            "    error \"Failed to set %%[a]s' maximum password age to %%[b]li\"\n"
		            "    bail 1\n", r->pwmax);
	if (ENFORCED(r, RES_USER_PWWARN))
		fprintf(io, "  ;;; password warning period\n"
		            "  set %%b \"%li\"\n"
		            "  user.set \"pwwarn\" %%b\n"
		            "  jz +2\n"
		            "    error \"Failed to set %%[a]s' password warning period to %%[b]li\"\n"
		            "    bail 1\n", r->pwwarn);
	if (ENFORCED(r, RES_USER_INACT))
		fprintf(io, "  ;;; password inactivity period\n"
		            "  set %%b \"%li\"\n"
		            "  user.set \"inact\" %%b\n"
		            "  jz +2\n"
		            "    error \"Failed to set %%[a]s' password inactivity period to %%[b]li\"\n"
		            "    bail 1\n", r->inact);
	if (ENFORCED(r, RES_USER_EXPIRE))
		fprintf(io, "  ;;; account expiration\n"
		            "  set %%b \"%s\"\n"
		            "  user.set \"expiry\" %%b\n"
		            "  jz +2\n"
		            "    error \"Failed to set %%[a]s' account expiration to %%[b]li\"\n"
		            "    bail 1\n", r->shell);
	if (ENFORCED(r, RES_USER_MKHOME))
		fprintf(io, "  flagged? \"mkhome\"\n"
		            "  jnz +2\n"
		            "    user.get \"home\" %%a\n"
		            "    set %%b \"%s\"\n"
		            "    user.get \"uid\" %%c\n"
		            "    user.get \"gid\" %%d\n"
		            "    call res.user.mkhome\n", r->skel);

	fprintf(io, "  call util.authdb.save\n");
	return 0;
}

FILE * res_user_content(const void *res, hash_t *facts) { return NULL; }


/*****************************************************************/

void* res_file_new(const char *key)
{
	return res_file_clone(NULL, key);
}

void* res_file_clone(const void *res, const char *key)
{
	const struct res_file *orig = (const struct res_file*)(res);
	struct res_file *rf = vmalloc(sizeof(struct res_file));

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

	return string("file:%s", rf->key);
}

int res_file_attrs(const void *res, hash_t *attrs)
{
	const struct res_file *rf = (const struct res_file*)(res);
	assert(rf); // LCOV_EXCL_LINE

	_hash_attr(attrs, "path", strdup(rf->path));
	_hash_attr(attrs, "present", strdup(ENFORCED(rf, RES_FILE_ABSENT) ? "no" : "yes"));

	_hash_attr(attrs, "owner", ENFORCED(rf, RES_FILE_UID) ? strdup(rf->owner) : NULL);
	_hash_attr(attrs, "group", ENFORCED(rf, RES_FILE_GID) ? strdup(rf->group) : NULL);
	_hash_attr(attrs, "mode", ENFORCED(rf, RES_FILE_MODE) ? string("%04o", rf->mode) : NULL);

	if (rf->verify) {
		_hash_attr(attrs, "verify", strdup(rf->verify));
		_hash_attr(attrs, "expect", string("%i", rf->expectrc));
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

int res_file_norm(void *res, struct policy *pol, hash_t *facts)
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
	hash_t *vars = vmalloc(sizeof(hash_t));
	hash_set(vars, "path", rf->path);

	if (rf->source) {
		char *x = interpolate(rf->source, vars);
		free(rf->source);
		rf->source = x;
	}
	if (rf->template) {
		char *x = interpolate(rf->template, vars);
		free(rf->template);
		rf->template = x;
	}
	hash_done(vars, 0);
	free(vars);

	/* derive tmpfile from path */
	if (!rf->tmpfile && rf->verify) {
		rf->tmpfile = vmalloc(strlen(rf->path) + 7);
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

		hash_t *vars = vmalloc(sizeof(hash_t));
		res_file_attrs(rf, vars);
		char *cmd = interpolate(rf->verify, vars);
		hash_done(vars, 1);
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
		test_value = string("%s", rf->path);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_file_gencode(const void *res, FILE *io)
{
	struct res_file *r = (struct res_file*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "  set %%a \"%s\"\n", r->path);

	if (ENFORCED(r, RES_FILE_ABSENT)) {
		fprintf(io, "  call res.file.absent\n");
		return 0;
	}

	fprintf(io, "  call res.file.present\n");
	if (ENFORCED(r, RES_FILE_UID))
		fprintf(io, "  set %%b \"%s\"\n"
		            "  call res.file.chown\n", r->owner);
	if (ENFORCED(r, RES_FILE_GID))
		fprintf(io, "  set %%b \"%s\"\n"
		            "  call res.file.chgrp\n", r->group);
	if (ENFORCED(r, RES_FILE_MODE))
		fprintf(io, "  set %%b 0%o\n"
		            "  call res.file.chmod\n", r->mode);
	if (ENFORCED(r, RES_FILE_SHA1)) {
		if (r->verify)
			fprintf(io, "  set %%b \"%s\"\n"
			            "  set %%d \"%s\"\n"
			            "  set %%e %u\n",
			            r->tmpfile, r->verify, r->expectrc);
		else
			fprintf(io, "  set %%b \"%s\"\n", r->path);

		fprintf(io, "  set %%c \"file:%s\"\n"
		            "  call res.file.contents\n", r->key);
	}
	return 0;
}

FILE * res_file_content(const void *res, hash_t *facts)
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
	struct res_symlink *r = vmalloc(sizeof(struct res_symlink));

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

	return string("symlink:%s", r->key);
}

int res_symlink_attrs(const void *res, hash_t *attrs)
{
	struct res_symlink *r = (struct res_symlink*)(res);
	assert(r); // LCOV_EXCL_LINE

	_hash_attr(attrs, "present", strdup(r->absent ? "no" : "yes"));
	_hash_attr(attrs, "path",    strdup(r->path));
	_hash_attr(attrs, "target",  strdup(r->target));
	return 0;
}

int res_symlink_norm(void *res, struct policy *pol, hash_t *facts)
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
		test_value = string("%s", r->path);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_symlink_gencode(const void *res, FILE *io)
{
	struct res_symlink *r = (struct res_symlink*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "  set %%a \"%s\"\n", r->path);

	if (r->absent)
		fprintf(io, "  call res.symlink.absent\n");
	else
		fprintf(io, "  set %%b \"%s\"\n"
		            "  call res.symlink.present\n", r->target);
	return 0;
}

FILE * res_symlink_content(const void *res, hash_t *facts) { return NULL; }


/*****************************************************************/

void* res_group_new(const char *key)
{
	return res_group_clone(NULL, key);
}

void* res_group_clone(const void *res, const char *key)
{
	const struct res_group *orig = (const struct res_group*)(res);
	struct res_group *rg = vmalloc(sizeof(struct res_group));

	rg->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

	rg->name   = RES_DEFAULT_STR(orig, name,   NULL);
	rg->passwd = RES_DEFAULT_STR(orig, passwd, NULL);

	rg->gid    = RES_DEFAULT(orig, gid, 0);

	/* FIXME: clone members of res_group */
	rg->mem_add = strings_new(NULL);
	rg->mem_rm  = strings_new(NULL);

	/* FIXME: clone admins of res_group */
	rg->adm_add = strings_new(NULL);
	rg->adm_rm  = strings_new(NULL);

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
			strings_free(rg->mem);
		}
		strings_free(rg->mem_add);
		strings_free(rg->mem_rm);

		if (rg->adm) {
			strings_free(rg->adm);
		}
		strings_free(rg->adm_add);
		strings_free(rg->adm_rm);
	}
	free(rg);
}

char* res_group_key(const void *res)
{
	const struct res_group *rg = (struct res_group*)(res);
	assert(rg); // LCOV_EXCL_LINE

	return string("group:%s", rg->key);
}

static char* _res_group_roster_mv(strings_t *add, strings_t *rm)
{
	char *added   = NULL;
	char *removed = NULL;
	char *final   = NULL;

	if (add->num > 0) {
		added = strings_join(add, " ");
	}
	if (rm->num > 0) {
		removed = strings_join(rm, " !");
	}

	if (added && removed) {
		final = string("%s !%s", added, removed);
		free(added);
		free(removed);
		added = removed = NULL;

	} else if (added) {
		final = added;

	} else if (removed) {
		final = string("!%s", removed);
		free(removed);
		removed = NULL;

	} else {
		final = strdup("");

	}
	return final;
}

int res_group_attrs(const void *res, hash_t *attrs)
{
	const struct res_group *rg = (const struct res_group*)(res);
	assert(rg); // LCOV_EXCL_LINE

	_hash_attr(attrs, "name", strdup(rg->name));
	_hash_attr(attrs, "gid", ENFORCED(rg, RES_GROUP_GID) ? string("%u",rg->gid) : NULL);
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

int res_group_norm(void *res, struct policy *pol, hash_t *facts) { return 0; }

int res_group_set(void *res, const char *name, const char *value)
{
	struct res_group *rg = (struct res_group*)(res);

	/* for multi-value attributes */
	strings_t *multi;
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
		multi = strings_split(value, strlen(value), " ", SPLIT_GREEDY);
		for_each_string(multi, i) {
			res_group_set(res, "member", multi->strings[i]);
		}
		strings_free(multi);

	} else if (strcmp(name, "admin") == 0) {
		if (value[0] == '!') {
			return res_group_remove_admin(rg, value+1);
		} else {
			return res_group_add_admin(rg, value);
		}

	} else if (strcmp(name, "admins") == 0) {
		multi = strings_split(value, strlen(value), " ", SPLIT_GREEDY);
		for_each_string(multi, i) {
			res_group_set(res, "admin", multi->strings[i]);
		}
		strings_free(multi);

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
		test_value = string("%u", rg->gid);
	} else if (strcmp(name, "name") == 0) {
		test_value = string("%s", rg->name);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_group_gencode(const void *res, FILE *io)
{
	struct res_group *r = (struct res_group*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "  call util.authdb.open\n"
	            "  set %%a \"%s\"\n", r->name);

	if (ENFORCED(r, RES_GROUP_ABSENT)) {
		fprintf(io, "  call res.group.absent\n"
		            "  call util.authdb.save\n");
		return 0;
	}

	fprintf(io, "  set %%b %u\n"
	            "  call res.group.present\n",
		ENFORCED(r, RES_GROUP_GID)? r->gid : 0);
	if (ENFORCED(r, RES_GROUP_PASSWD))
		fprintf(io, "  set %%b \"%s\"\n"
		            "  call res.group.passwd\n", r->passwd);
	if (ENFORCED(r, RES_GROUP_MEMBERS)) {
		fprintf(io, "  ;; FIXME: manage group membership!\n");
	}
	if (ENFORCED(r, RES_GROUP_ADMINS)) {
		fprintf(io, "  ;; FIXME: manage group adminhood!\n");
	}
	fprintf(io, "  call util.authdb.save\n");
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

FILE * res_group_content(const void *res, hash_t *facts) { return NULL; }

/*****************************************************************/

void* res_package_new(const char *key)
{
	return res_package_clone(NULL, key);
}

void* res_package_clone(const void *res, const char *key)
{
	const struct res_package *orig = (const struct res_package*)(res);
	struct res_package *rp = vmalloc(sizeof(struct res_package));

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

	return string("package:%s", rp->key);
}

int res_package_attrs(const void *res, hash_t *attrs)
{
	const struct res_package *rp = (const struct res_package*)(res);
	assert(rp); // LCOV_EXCL_LINE

	_hash_attr(attrs, "name", cw_strdup(rp->name));
	_hash_attr(attrs, "version", cw_strdup(rp->version));
	_hash_attr(attrs, "installed", strdup(ENFORCED(rp, RES_PACKAGE_ABSENT) ? "no" : "yes"));
	return 0;
}

int res_package_norm(void *res, struct policy *pol, hash_t *facts) { return 0; }

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
		test_value = string("%s", rp->name);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_package_gencode(const void *res, FILE *io)
{
	struct res_package *r = (struct res_package*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "  set %%a \"%s\"\n", r->name);
	if (ENFORCED(r, RES_PACKAGE_ABSENT)) {
		fprintf(io, "  call res.package.absent\n");
		return 0;
	}

	if (r->version)     fprintf(io, "  set %%b \"%s\"\n", r->version);
	else if (r->latest) fprintf(io, "  set %%b \"latest\"\n");
	else                fprintf(io, "  set %%b \"\"\n");
	fprintf(io, "  call res.package.install\n");
	return 0;
}

FILE * res_package_content(const void *res, hash_t *facts) { return NULL; }

/*****************************************************************/

void* res_service_new(const char *key)
{
	return res_service_clone(NULL, key);
}

void* res_service_clone(const void *res, const char *key)
{
	const struct res_service *orig = (const struct res_service*)(res);
	struct res_service *rs = vmalloc(sizeof(struct res_service));

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

	return string("service:%s", rs->key);
}

int res_service_attrs(const void *res, hash_t *attrs)
{
	const struct res_service *rs = (const struct res_service*)(res);
	assert(rs); // LCOV_EXCL_LINE

	_hash_attr(attrs, "name", cw_strdup(rs->service));
	_hash_attr(attrs, "running", strdup(ENFORCED(rs, RES_SERVICE_RUNNING) ? "yes" : "no"));
	_hash_attr(attrs, "enabled", strdup(ENFORCED(rs, RES_SERVICE_ENABLED) ? "yes" : "no"));
	_hash_attr(attrs, "notify", cw_strdup(rs->notify));
	return 0;
}

int res_service_norm(void *res, struct policy *pol, hash_t *facts) { return 0; }

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
		test_value = string("%s", rs->service);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_service_gencode(const void *res, FILE *io)
{
	struct res_service *r = (struct res_service*)(res);
	assert(r); // LCOV_EXCL_LINE
	fprintf(io, "  set %%a \"%s\"\n", r->service);

	if (ENFORCED(r, RES_SERVICE_ENABLED))
		fprintf(io, "  call res.service.enable\n");
	else if (ENFORCED(r, RES_SERVICE_DISABLED))
		fprintf(io, "  call res.service.disable\n");

	if (ENFORCED(r, RES_SERVICE_RUNNING))
		fprintf(io, "  call res.service.start\n");
	else if (ENFORCED(r, RES_SERVICE_STOPPED))
		fprintf(io, "  call res.service.stop\n");

	if (ENFORCED(r, RES_SERVICE_RUNNING))
		fprintf(io, "  flagged? \"service:%s\"\n"
		            "  jz +1 ret\n"
		            "  call res.service.%s\n",
		            r->key, r->notify ? r->notify : "restart");
	return 0;
}

FILE * res_service_content(const void *res, hash_t *facts) { return NULL; }

/*****************************************************************/

void* res_host_new(const char *key)
{
	return res_host_clone(NULL, key);
}

void* res_host_clone(const void *res, const char *key)
{
	const struct res_host *orig = (const struct res_host*)(res);
	struct res_host *rh = vmalloc(sizeof(struct res_host));

	rh->enforced  = RES_DEFAULT(orig, enforced, RES_NONE);

	/* FIXME: clone host aliases */
	rh->aliases = strings_new(NULL);

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
		strings_free(rh->aliases);

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

int res_host_attrs(const void *res, hash_t *attrs)
{
	const struct res_host *rh = (const struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE

	_hash_attr(attrs, "hostname", cw_strdup(rh->hostname));
	_hash_attr(attrs, "present",  strdup(ENFORCED(rh, RES_HOST_ABSENT) ? "no" : "yes"));
	_hash_attr(attrs, "ip", cw_strdup(rh->ip));
	if (ENFORCED(rh, RES_HOST_ALIASES)) {
		_hash_attr(attrs, "aliases", strings_join(rh->aliases, " "));
	} else {
		_hash_attr(attrs, "aliases", NULL);
	}
	return 0;
}

int res_host_norm(void *res, struct policy *pol, hash_t *facts) { return 0; }

int res_host_set(void *res, const char *name, const char *value)
{
	struct res_host *rh = (struct res_host*)(res);
	assert(rh); // LCOV_EXCL_LINE
	strings_t *alias_tmp;

	if (strcmp(name, "hostname") == 0) {
		free(rh->hostname);
		rh->hostname = strdup(value);

	} else if (strcmp(name, "ip") == 0 || strcmp(name, "address") == 0) {
		free(rh->ip);
		rh->ip = strdup(value);

	} else if (strcmp(name, "aliases") == 0 || strcmp(name, "alias") == 0) {
		alias_tmp = strings_split(value, strlen(value), " ", SPLIT_GREEDY);
		if (strings_add_all(rh->aliases, alias_tmp) != 0) {
			strings_free(alias_tmp);
			return -1;
		}
		strings_free(alias_tmp);

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

int res_host_gencode(const void *res, FILE *io)
{
	struct res_host *r = (struct res_host*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "  set %%a \"%s\"\n"
	            "  set %%b \"%s\"\n", r->ip, r->hostname);

	if (ENFORCED(r, RES_HOST_ABSENT)) {
		fprintf(io, "  call res.host.absent\n");
	} else {
		fprintf(io, "  call res.host.present\n");

		if (ENFORCED(r, RES_HOST_ALIASES)) {
			fprintf(io, "  call res.host.clear-aliases\n"
			            "  set %%c 0\n");
			int i;
			for (i = 0; i < r->aliases->num; i++)
				fprintf(io, "  set %%d \"%s\"\n"
				            "  call res.host.add-alias\n", r->aliases->strings[i]);
		}
	}
	return 0;
}

FILE * res_host_content(const void *res, hash_t *facts) { return NULL; }

/*****************************************************************/

void* res_dir_new(const char *key)
{
	return res_dir_clone(NULL, key);
}

void* res_dir_clone(const void *res, const char *key)
{
	const struct res_dir *orig = (const struct res_dir*)(res);
	struct res_dir *rd = vmalloc(sizeof(struct res_dir));

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

	return string("dir:%s", rd->key);
}

int res_dir_attrs(const void *res, hash_t *attrs)
{
	const struct res_dir *rd = (const struct res_dir*)(res);
	assert(rd); // LCOV_EXCL_LINE

	_hash_attr(attrs, "path", cw_strdup(rd->path));
	_hash_attr(attrs, "owner", ENFORCED(rd, RES_DIR_UID) ? strdup(rd->owner) : NULL);
	_hash_attr(attrs, "group", ENFORCED(rd, RES_DIR_GID) ? strdup(rd->group) : NULL);
	_hash_attr(attrs, "mode", ENFORCED(rd, RES_DIR_MODE) ? string("%04o", rd->mode) : NULL);
	_hash_attr(attrs, "present", strdup(ENFORCED(rd, RES_DIR_ABSENT) ? "no" : "yes"));
	return 0;
}

int res_dir_norm(void *res, struct policy *pol, hash_t *facts)
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

int res_dir_gencode(const void *res, FILE *io)
{
	struct res_dir *r = (struct res_dir*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "  set %%a \"%s\"\n", r->path);
	if (ENFORCED(r, RES_DIR_ABSENT)) {
		fprintf(io, "  call res.dir.absent\n");
		return 0;
	}

	fprintf(io, "  call res.dir.present\n");
	if (ENFORCED(r, RES_DIR_UID))
		fprintf(io, "  set %%b \"%s\"\n"
		            "  call res.file.chown\n", r->owner);
	if (ENFORCED(r, RES_DIR_GID))
		fprintf(io, "  set %%b \"%s\"\n"
		            "  call res.file.chgrp\n", r->group);
	if (ENFORCED(r, RES_DIR_MODE))
		fprintf(io, "  set %%b 0%o\n"
		            "  call res.file.chmod\n", r->mode);
	return 0;
}

FILE * res_dir_content(const void *res, hash_t *facts) { return NULL; }

/********************************************************************/

void* res_exec_new(const char *key)
{
	return res_exec_clone(NULL, key);
}

void* res_exec_clone(const void *res, const char *key)
{
	const struct res_exec *orig = (const struct res_exec*)(res);
	struct res_exec *re = vmalloc(sizeof(struct res_exec));

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

	return string("exec:%s", re->key);
}

int res_exec_attrs(const void *res, hash_t *attrs)
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

int res_exec_norm(void *res, struct policy *pol, hash_t *facts)
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
		test_value = string("%s", re->command);
	} else {
		return 1;
	}

	rc = strcmp(test_value, value);
	free(test_value);
	return rc;
}

int res_exec_gencode(const void *res, FILE *io)
{
	struct res_exec *r = (struct res_exec*)(res);
	assert(r); // LCOV_EXCL_LINE

	fprintf(io, "  set %%b \"%s\"\n"
	            "  runas.uid 0\n"
	            "  runas.gid 0\n", r->command);
	if (ENFORCED(r, RES_EXEC_UID) || ENFORCED(r, RES_EXEC_GID)) {
		fprintf(io, "  call util.authdb.open\n");
		if (ENFORCED(r, RES_EXEC_UID))
			fprintf(io, "  set %%a \"%s\"\n"
			            "  call util.runuser\n", r->user);
		if (ENFORCED(r, RES_EXEC_GID))
			fprintf(io, "  set %%a \"%s\"\n"
			            "  call util.rungroup\n", r->group);
		fprintf(io, "  call util.authdb.close\n");
	}

	if (ENFORCED(r, RES_EXEC_TEST))
		fprintf(io, "  set %%c \"%s\"\n"
		            "  exec %%c %%d\n"
		            "  jz +1 ret\n", r->test);

	if (ENFORCED(r, RES_EXEC_ONDEMAND))
		fprintf(io, "  flagged? \"%s\"\n"
		            "  jz +1 ret\n", r->key);

	fprintf(io, "  exec %%b %%d\n");
	return 0;
}

FILE * res_exec_content(const void *res, hash_t *facts) { return NULL; }
