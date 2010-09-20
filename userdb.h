#ifndef USERDB_H
#define USERDB_H

#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>

struct pwdb;
struct spdb;

struct pwdb {
	struct pwdb   *next;
	struct passwd *passwd;
};

struct spdb {
	struct spdb *next;
	struct spwd *spwd;
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

#endif
