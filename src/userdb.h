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

#ifndef USERDB_H
#define USERDB_H

#include "clockwork.h"

#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <grp.h>

/* ripped from some version of glibc, >2.5 */
struct sgrp {
	char  *sg_namp;
	char  *sg_passwd;
	char **sg_adm;
	char **sg_mem;
};

/**
  /etc/passwd Database
 */
struct pwdb {
	struct pwdb   *next;     /* the next database entry */
	struct passwd *passwd;   /* user account record */
};

/**
  /etc/shadow Database
 */
struct spdb {
	struct spdb *next;       /* the next database entry */
	struct spwd *spwd;       /* shadow account record */
};

/**
  /etc/group Database
 */
struct grdb {
	struct grdb  *next;      /* the next database entry */
	struct group *group;     /* group account record */
};

/**
  /etc/gshadow Database
 */
struct sgdb {
	struct sgdb *next;       /* the next database entry */
	struct sgrp *sgrp;       /* shadow account record */
};

struct pwdb* pwdb_init(const char *path);
struct passwd* pwdb_get_by_name(struct pwdb *db, const char *name);
struct passwd* pwdb_get_by_uid(struct pwdb *db, uid_t uid);
int pwdb_lookup_uid(struct pwdb *db, const char *name, uid_t *uid);
uid_t pwdb_next_uid(struct pwdb *db);
struct passwd* pwdb_new_entry(struct pwdb *db, const char *name, uid_t uid, gid_t gid);
int pwdb_add(struct pwdb *db, struct passwd *pw);
int pwdb_rm(struct pwdb *db, struct passwd *pw);
int pwdb_write(struct pwdb *db, const char *file);
void pwdb_free(struct pwdb *db);

struct spdb* spdb_init(const char *path);
struct spwd* spdb_get_by_name(struct spdb *db, const char *name);
struct spwd* spdb_new_entry(struct spdb *db, const char *name);
int spdb_add(struct spdb *db, struct spwd *sp);
int spdb_rm(struct spdb *db, struct spwd *sp);
int spdb_write(struct spdb *db, const char *file);
void spdb_free(struct spdb *db);

struct grdb* grdb_init(const char *path);
struct group* grdb_get_by_name(struct grdb *db, const char *name);
int grdb_lookup_gid(struct grdb *db, const char *name, gid_t *gid);
struct group* grdb_get_by_gid(struct grdb *db, gid_t gid);
struct group* grdb_new_entry(struct grdb *db, const char *name, gid_t gid);
int grdb_add(struct grdb *db, struct group *g);
int grdb_rm(struct grdb *db, struct group *g);
int grdb_write(struct grdb *db, const char *file);
void grdb_free(struct grdb *db);

struct sgdb* sgdb_init(const char *path);
struct sgrp* sgdb_get_by_name(struct sgdb *db, const char *name);
struct sgrp* sgdb_new_entry(struct sgdb *db, const char *name);
int sgdb_add(struct sgdb *db, struct sgrp *g);
int sgdb_rm(struct sgdb *db, struct sgrp *g);
int sgdb_write(struct sgdb *db, const char *file);
void sgdb_free(struct sgdb *db);

#endif
