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

#ifndef USERDB_H
#define USERDB_H

#include "clockwork.h"

#include <sys/types.h>
#include <pwd.h>
#include <shadow.h>
#include <grp.h>
#include <gshadow.h>

/** @file userdb.h

  The UserDB module defines structures and methods for querying
  and manipulating data stored in the user, shadow, group and
  gshadow databases.

  Each of the four primary datatypes (pwdb, spdb, grdb and spdb)
  represents a single entry from the four key databases:

  - pwdb - /etc/passwd
  - spdb - /etc/shadow
  - grdb - /etc/group
  - sgdb - /etc/gshadow

  While all four of these data types store the data for only one
  record, each also has a pointer to the next record in the file,
  forming a singly-linked list of records.

 */

/**
  User database entry, representing the records stored in
  the /etc/passwd user database.
 */
struct pwdb {
	/** Link to the next pwdb entry. */
	struct pwdb   *next;
	/** Pointer to a user account record. */
	struct passwd *passwd;
};

/**
  Shadow password entry, representing the records
  stored in the /etc/shadow password database.
 */
struct spdb {
	/** Link to the next spdb entry. */
	struct spdb *next;
	/** Pointer to a shadow account record. */
	struct spwd *spwd;
};

/**
  Group database entry, representing the records stored
  in the /etc/group database.
 */
struct grdb {
	/** Link to the next grdb entry. */
	struct grdb  *next;
	/** Pointer to a group account record. */
	struct group *group;
};

/**
  GShadow database entry, representing the records stored
  in the /etc/gshadow database.
 */
struct sgdb {
	/** Link to the next sgdb entry. */
	struct sgdb *next;
	/** Pointer to a group account gshadow record. */
	struct sgrp *sgrp;
};

/**
  Allocate and initialize a new user database, reading and
  parsing records from \a path.

  @note The pointer returned by this function must be passed to
        pwdb_free in order to properly release all memory allocated
        dynamically on the heap.

  @param  path    Path to the passwd database (usually /etc/passwd).

  @returns a pointer to the head of a list of pwdb user database
           entry structures.
 */
struct pwdb* pwdb_init(const char *path);

/**
  Look up a user account, by username, in a user database.

  @param  db      User database to search.
  @param  name    Username of the account to look for.

  @returns a pointer to a passwd structure (see getpwent(3)) if the
           user can be found, otherwise NULL.
 */
struct passwd* pwdb_get_by_name(struct pwdb *db, const char *name);

/**
  Look up a UID, by username, in a user database.

  @param  db      User database to search.
  @param  name    Username of the account to look for.

  @returns the UID of the named account, if the entry
           was found, otherwise -1.
 */
uid_t pwdb_lookup_uid(struct pwdb *db, const char *name);

/**
  Find an unused UID in a user database

  Useful for creating "auto-increment" UIDs, pwdb_next_uid
  starts at UID 1000 and tries to find the next available
  user ID, based on what IDs have already been handed out.

  It emulates the automatic UID generation of the standard
  useradd command (i.e. highest in use + 1)

  @param  db      User database to search.
  @returns an available UID.
 */
uid_t pwdb_next_uid(struct pwdb *db);

/**
  Look up a user account, by UID, in a user database.

  @param  db     User database to search.
  @param  uid    UID of the account to look for.

  @returns a pointer to a passwd structure (see getpwent(3)) if the
           user can be found, otherwise NULL.
 */
struct passwd* pwdb_get_by_uid(struct pwdb *db, uid_t uid);

/**
  Create and return a new user database entry.

  @see spdb_new_entry

  @param  db      Database to create the new entry in.
  @param  name    Username of the new entry.
  @param  uid     UNIX User ID of new entry.
  @param  gid     UNIX Group ID of new entry.

  @returns a pointer to the newly created passwd structure
           (see getpwent(3)).
 */
struct passwd* pwdb_new_entry(struct pwdb *db, const char *name, uid_t uid, gid_t gid);

/**
  Add a new user entry to a user database.

  @note If an entry already exists in \a db that has the same
        UID as \a pw, the add operation "fails" and returns
        a suitable failure value (-1).

  @param  db   Database to add the entry to.
  @param  pw   User entry to add.

  @returns 0 on success, -1 on failure.
 */
int pwdb_add(struct pwdb *db, struct passwd *pw);

/**
  Remove an entry from a user database.

  This function evaluates each entry by comparing the passwd
  pointer to the \a pw argument.  For that reason, callers must
  use a pointer returned by pwdb_get_by_name or pwdb_get_by_uid;
  it is not sufficient to populate a new passwd structure and
  pass it to this function.

  @param  db    Database to remove the entry from.
  @param  pw    Pointer to the passwd entry to remove.

  @returns 0 on success, non-zero on failure.
 */
int pwdb_rm(struct pwdb *db, struct passwd *pw);

/**
  Write a user database to a file.

  @param  db      Database to store.
  @param  file    Path to the file to store entries in.

  @returns 0 on success, non-zero on failure.
 */
int pwdb_write(struct pwdb *db, const char *file);

/**
  De-allocate a user database and free its memory resources.

  @param  db    Database to de-allocate.
 */
void pwdb_free(struct pwdb *db);

/**
  Allocate and initialize a new shadow database, reading and
  parsing records from \a path.

  @note The pointer returned by this function must be passed to
        spdb_free in order to properly release all memory allocated
        dynamically on the heap.

  @param  path    Path to the shadow database (usually /etc/shadow).

  @returns a pointer to the head of a list of spdb password database
           entry structures.
 */
struct spdb* spdb_init(const char *path);

/**
  Look up a shadow entry, by username, in a shadow database.

  @param  db      Shadow database to search.
  @param  name    Username of the account to look for.

  @returns a pointer to an spwd structure (see getspent(3)) if the
           entry can be found, otherwise NULL.
 */
struct spwd* spdb_get_by_name(struct spdb *db, const char *name);

/**
  Create and return a new shadow database entry.

  @param  db      Database to create the new entry in
  @param  name    Username of the new entry.

  @returns a pointer to the newly created spwd structure
           (see getspent(3)).
 */
struct spwd* spdb_new_entry(struct spdb *db, const char *name);

/**
  Add a new shadow entry to a shadow database.

  @note If an entry already exists in \a db that has the same
        UID as \a pw, the add operation "fails" and returns a
        suitable failure value (-1).

  @param  db   Database to add the entry to.
  @param  sp   Shadow entry to add.

  @returns 0 on success, -1 on failure.
 */
int spdb_add(struct spdb *db, struct spwd *sp);

/**
  Remove an entry from a shadow database.

  This function evaluates each entry by comparing the spwd
  pointer to the \a sp argument.  For that reason, callers must
  use a pointer returned by spdb_get_by_name; it is not sufficient
  to populate a new spwd structure and pass it to this function.

  @param  db    Database to remove the entry from.
  @param  sp    Pointer to the spwd entry to remove.

  @returns 0 on success, non-zero on failure.
 */
int spdb_rm(struct spdb *db, struct spwd *sp);

/**
  Write a shadow database to a file.

  @param  db      Database to store.
  @param  file    Path to the file to store entries in.

  @returns 0 on success, non-zero on failure.
 */
int spdb_write(struct spdb *db, const char *file);

/**
  De-allocate a shadow database and free its memory resources.

  @param  db    Database to de-allocate.
 */
void spdb_free(struct spdb *db);

/**
  Allocate and initialize a new group database, reading and
  parsing records from \a path.

  @note The pointer returned by this function must be passed to
        grdb_free in order to properly release all memory allocated
        dynamically on the heap.

  @param  path    Path to the group database (usually /etc/group).

  @returns a pointer to the head of a list of grdb group database
           entry structures.
 */
struct grdb* grdb_init(const char *path);

/**
  Look up a group entry, by name, in a group database.

  @param  db      Group database to search.
  @param  name    Name of the group account to look for.

  @returns a pointer to a group structure (see getgrent(3)) if the
           entry was found, otherwise NULL.
 */
struct group* grdb_get_by_name(struct grdb *db, const char *name);

/**
  Look up a group ID, by name, in a group database.

  @param  db      Group database to search.
  @param  name    Name of group account to look for.

  @returns the GID of the group if found, otherwise -1.
 */
gid_t grdb_lookup_gid(struct grdb *db, const char *name);

/** Look up a group entry, by GID, in a group database.

  @param  db     Group database to search.
  @param  gid    ID of the group account to look for.

  @returns a pointer to a group structure (see getgrent(3)) if the
           entry was found, otherwise NULL.
 */
struct group* grdb_get_by_gid(struct grdb *db, gid_t gid);

/**
  Create and return a new group database entry.

  @param  db      Database to create the new entry in.
  @param  name    Name of the new group entry.
  @param  gid     UNIX Group ID of the new entry.

  @returns a pointer to the newly created group structure
           (see getgrent(3)).
 */
struct group* grdb_new_entry(struct grdb *db, const char *name, gid_t gid);

/**
  Add a group entry to a group database.

  @note If a group entry already exists in \a db with
        the same GID as \a g, the add operation "fails".

  @param  db    Database to add the entry to.
  @param  g     Group entry to add.

  @returns 0 on success, -1 on failure.
 */
int grdb_add(struct grdb *db, struct group *g);

/**
  Remove an entry from a group database.

  This function evaluates each entry by comparing the group
  pointer to the \a g argument.  For that reason, callers must
  use a pointer returned by grdb_get_by_name or grdb_get_by_gid;
  it is not sufficient to populate a new group structure and
  pass it to this function.

  @param  db    Database to remove the entry from.
  @param  g     Pointer to the group structure to remove.

  @returns 0 on success, non-zero on failure.
 */
int grdb_rm(struct grdb *db, struct group *g);

/**
  Write a group database to a file.

  @param  db      Database to store.
  @param  file    Path to the file to store entries in.

  @returns 0 on success, non-zero on failure.
 */
int grdb_write(struct grdb *db, const char *file);

/**
  De-allocate a group database and free its memory resources.

  @param  db    Database to de-allocate.
 */
void grdb_free(struct grdb *db);

/**
  Allocate and initialize a new group shadow database, reading
  and parsing records from \a path.

  @note The pointer returned by this function must be passed to
        sgdb_free in order to properly release all memory allocated
        dynamically on the heap.

  @param  path    Path to the gshadow database (usually /etc/gshadow).

  @returns a pointer to the head of a list of sgdb group shadow
           database entry structures.
 */
struct sgdb* sgdb_init(const char *path);

/**
  Look up a group shadow entry, by username, in a group shadow
  database.

  @param  db      Group shadow database to search.
  @param  name    Name of the group account to lok for.

  @returns a pointer to an sgrp structure if the entry was found,
           otherwise NULL.
 */
struct sgrp* sgdb_get_by_name(struct sgdb *db, const char *name);

/** Create and return a new group shadow database entry.

  @param  db      Database to create the new entry in.
  @param  name    Name of the new group entry.

  @returns a pointer to the newly created sgrp structure.
 */
struct sgrp* sgdb_new_entry(struct sgdb *db, const char *name);

/**
  Add a group shadow entry to a group shadow database.

  @note If a group entry already exists in \a db with
        the same GID as \a g, the add operation "fails".

  @param  db    Database to add the entry to.
  @param  g     Group shadow entry to add.

  @returns 0 on success, -1 on failure.
 */
int sgdb_add(struct sgdb *db, struct sgrp *g);

/** Remove an entry from a group shadow database.

  This function evaluates each entry by comparing the sgrp
  pointer to the \a g argument.  For that reason, callers must
  use a pointer returned by sgdb_get_by_name; it is not sufficient
  to populate a new sgrp structure and pass it to this function.

  @param  db    Database to remove the entry from.
  @param  g     Pointer to the sgrp structure to remove.

  @returns 0 on success, non-zero on failure.
 */
int sgdb_rm(struct sgdb *db, struct sgrp *g);

/**
  Write a group shadow database to a file.

  @param  db      Database to store.
  @param  file    Path to the file to store entries in.

  @returns 0 on success, non-zero on failure.
 */
int sgdb_write(struct sgdb *db, const char *file);

/**
  De-allocate a group shadow database and free its memory resources.

  @param  db    Database to de-allocate.
 */
void sgdb_free(struct sgdb *db);

#endif
