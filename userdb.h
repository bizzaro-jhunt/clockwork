#ifndef USERDB_H
#define USERDB_H

#include "env.h"

#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <grp.h>
#include <gshadow.h>

struct pwdb;
struct spdb;
struct grdb;

struct pwdb {
	struct pwdb   *next;
	struct passwd *passwd;
};

struct spdb {
	struct spdb *next;
	struct spwd *spwd;
};

struct grdb {
	struct grdb  *next;
	struct group *group;
};

struct sgdb {
	struct sgdb *next;
	struct sgrp *sgrp;
};

struct pwdb* pwdb_init(const char *path);
struct passwd* pwdb_get_by_name(struct pwdb *db, const char *name); /* FIXME: needs const qualifier */
struct passwd* pwdb_get_by_uid(struct pwdb *db, uid_t uid); /* FIXME: needs const qualifier */
int pwdb_add(struct pwdb *db, struct passwd *pw); /* FIXME: needs const qualifier */
int pwdb_rm(struct pwdb *db, struct passwd *pw); /* FIXME: needs const qualifier */
int pwdb_write(struct pwdb *db, const char *file);
void pwdb_free(struct pwdb *db);

struct spdb* spdb_init(const char *path);
struct spwd* spdb_get_by_name(struct spdb *db, const char *name); /* FIXME: needs const qualifier */
int spdb_add(struct spdb *db, struct spwd *sp); /* FIXME: needs const qualifier */
int spdb_rm(struct spdb *db, struct spwd *sp); /* FIXME: needs const qualifier */
int spdb_write(struct spdb *db, const char *file);
void spdb_free(struct spdb *db);

struct grdb* grdb_init(const char *path);
struct group* grdb_get_by_name(struct grdb *db, const char *name);
struct group* grdb_get_by_gid(struct grdb *db, gid_t gid);
int grdb_add(struct grdb *db, struct group *g);
int grdb_rm(struct grdb *db, struct group *g);
int grdb_write(struct grdb *db, const char *file);
void grdb_free(struct grdb *db);

struct sgdb* sgdb_init(const char *path);
struct sgrp* sgdb_get_by_name(struct sgdb *db, const char *name);
int sgdb_add(struct sgdb *db, struct sgrp *g);
int sgdb_rm(struct sgdb *db, struct sgrp *g);
int sgdb_write(struct sgdb *db, const char *file);
void sgdb_free(struct sgdb *db);

#endif
