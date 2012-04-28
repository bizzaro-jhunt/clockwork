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

/** Type of database (agent or master) */
enum db_type {
	MASTERDB = 1,
	AGENTDB  = 2
};

/* use old-school sqlite functions before 3.3.9 */
#if SQLITE_VERSION_NUMBER <= 3003009
#  define DB_SQLITE3_OLD_SCHOOL 1
#endif

/**
  Database Handle
 */
struct db {
	enum db_type   db_type; /* what type of db is this? */
	char          *path;    /* path to the SQLite3 db file */
	sqlite3       *db;      /* SQLite3 handle (opaque pointer) */
};

/* C integral type used for record primary keys. */
#ifdef DB_SQLITE3_OLD_SCHOOL
typedef sqlite_int64 rowid;
#else
typedef sqlite3_int64 rowid;
#endif
#define NULL_ROWID (rowid)(-1)

struct db* db_open(enum db_type type, const char *path);
int db_close(struct db *db);

/* Policy Master tasks:
   create host
   store report

   Clockwork Agent tasks:
   store report
   [retrieve stats]

   everything else will be done in perl?
   */

rowid masterdb_host(struct db *db, const char *host);
int masterdb_store_report(struct db *db, rowid host_id, struct job *job);
int agentdb_store_report(struct db *db, struct job *job);

#endif
