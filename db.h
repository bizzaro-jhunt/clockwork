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

#ifndef DB_H
#define DB_H

#include "clockwork.h"
#include <sqlite3.h>
#include <sys/time.h>

#include "job.h"

/** @file db.h
  Database Routines

  This module contains structures and functions for interfacing
  with the Clockwork reporting databases for agents and the
  policy master.

  The current implementation is strictly limited to the SQLite3
  database platform, but it should be adaptable to others.
 */

/** Type of database (agent or master) */
enum db_type {
	DB_MASTER = 1,
	DB_AGENT  = 2
};

/** Database Handle */
typedef struct {
	/** Type of database represented by this handle. */
	enum db_type db_type;

	/** Path to the SQLite3 database fie */
	const char *path;

	/** SQLite3 handle, an opaque pointer. */
	sqlite3 *db;
} DB;

/** C integral type used for record primary keys. */
typedef sqlite3_int64 rowid;
/** A non-rowid (invalid) value.  Used for signifying error conditions. */
#define NULL_ROWID (rowid)(-1)

/**
  Open a SQLite3 database "connection"

  @param  type    Type of database that \a path points to
  @param  path    Path to the SQLite3 database file.

  @returns a DB handle on success, or NULL on failure.
 */
DB* db_open(enum db_type type, const char *path);

/**
  Close an open SQLite3 database handle.

  @param  db   Database handle to close.

  @returns 0 on success, non-zero on failure.
 */
int db_close(DB *db);

/* Policy Master tasks:
   create host
   store report

   Clockwork Agent tasks:
   store report
   [retrieve stats]

   everything else will be done in perl?
   */

/**
  Look up a host's rowid by its name.

  This function only makes sense if \a db is a
  policy master database handle.

  @param  db     Database handle to search against.
  @param  host   FQDN of the host to search for.

  @returns the rowid of the host, if found, or NULL_ROWID
           if not found.
 */
rowid masterdb_host(DB *db, const char *host);

/**
  Store a host job report in the policy master database.

  This function only makes sense if \a db is a
  policy master database handle.

  @param  db       Database handle to insert into.
  @param  host_id  rowid of the host.
  @param  job      Job report to store.

  @returns 0 on success, non-zero on failure.
 */
int masterdb_store_report(DB *db, rowid host_id, struct job *job);

/**
  Store a job report in the local agent database.

  THis function only makes sense if \a db is an
  agent database handle.

  @param  db     Database handle to insert into.
  @param  job    Job report to store.

  @returns 0 on success, non-zero on failure.
 */
int agentdb_store_report(DB *db, struct job *job);

#endif
