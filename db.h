#ifndef DB_H
#define DB_H

#include "clockwork.h"
#include <sqlite3.h>
#include <sys/time.h>

#include "job.h"

enum db_type {
	DB_MASTER = 1,
	DB_AGENT  = 2
};

typedef struct {
	enum db_type db_type;
	const char *path;
	sqlite3 *db;
} DB;

typedef sqlite3_int64 rowid;
#define NULL_ROWID (rowid)(-1)

DB* db_open(enum db_type type, const char *path);
int db_close(DB *db);

/* Policy Master tasks:
   create host
   store report

   Clockwork Agent tasks:
   store report
   [retrieve stats]

   everything else will be done in perl?
   */

rowid masterdb_host(DB *db, const char *host);
int masterdb_store_report(DB *db, rowid host_id, struct job *job);
int agentdb_store_report(DB *db, struct job *job);


#endif
