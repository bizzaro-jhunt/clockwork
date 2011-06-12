#ifndef DB_H
#define DB_H

#include "clockwork.h"
#include <sqlite3.h>
#include <sys/time.h>

#include "job.h"

enum reportdb_type {
	DB_MASTER = 1,
	DB_AGENT  = 2
};

struct reportdb {
	enum reportdb_type db_type;
	const char *path;
	sqlite3 *db;
};

typedef sqlite3_int64 rowid;
#define NULL_ROWID (rowid)(-1)

struct reportdb* reportdb_open(enum reportdb_type type, const char *path);
int reportdb_close(struct reportdb *db);

/* Policy Master tasks:
   create host
   store report

   Clockwork Agent tasks:
   store report
   [retrieve stats]

   everything else will be done in perl?
   */

rowid masterdb_host(struct reportdb *db, const char *host);
int masterdb_store_report(struct reportdb *db, rowid host_id, struct job *job);
int agentdb_store_report(struct reportdb *db, struct job *job);


#endif
