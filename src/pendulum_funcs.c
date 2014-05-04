#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <pwd.h>
#include <shadow.h>
#include <grp.h>

#include "pendulum_funcs.h"
//#include "userdb.h"

struct pwdb {
	struct pwdb   *next;     /* the next database entry */
	struct passwd *passwd;   /* user account record */
};

typedef struct {
	struct pwdb   *pwdb;
	struct passwd *pwent;
} udata;

#define UDATA(m) ((udata*)(m->U))

/***************************************************/

static pn_word cwa_fs_is_file(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISREG(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_dir(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISDIR(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_chardev(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISCHR(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_blockdev(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISBLK(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_fifo(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISFIFO(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_symlink(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISLNK(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_is_socket(pn_machine *m)
{
	struct stat st;
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISSOCK(st.st_mode) ? 0 : 1;
}

static pn_word cwa_fs_exists(pn_machine *m)
{
	struct stat st;
	return lstat((const char *)m->A, &st) == 0 ? 0 : 1;
}

static pn_word cwa_fs_mkdir(pn_machine *m)
{
	return mkdir((const char *)m->A, 0777) == 0 ? 0 : 1;
}

static pn_word cwa_fs_mkfile(pn_machine *m)
{
	int fd = open((const char *)m->A, O_WRONLY|O_CREAT, 0644);
	if (fd < 0) return 1;
	close(fd);
	return 0;
}

static pn_word cwa_fs_symlink(pn_machine *m)
{
	return symlink((const char *)m->A, (const char *)m->B) == 0 ? 0 : 1;
}

static pn_word cwa_fs_link(pn_machine *m)
{
	return link((const char *)m->A, (const char *)m->B) == 0 ? 0 : 1;
}

static pn_word cwa_fs_chown(pn_machine *m)
{
	return chown((const char *)m->A, m->B, m->C) == 0 ? 0 : 1;
}

static pn_word cwa_fs_chmod(pn_machine *m)
{
	return chmod((const char *)m->A, m->D) == 0 ? 0 : 1;
}

static pn_word cwa_fs_unlink(pn_machine *m)
{
	return unlink((const char *)m->A) == 0 ? 0 : 1;
}

static pn_word cwa_fs_rmdir(pn_machine *m)
{
	return rmdir((const char *)m->A) == 0 ? 0 : 1;
}

/***************************************************/

static struct pwdb* s_fgetpwent(FILE *input)
{
	assert(input);

	struct passwd *passwd = fgetpwent(input);
	if (!passwd) return NULL;

	struct pwdb *ent;
	ent = calloc(1, sizeof(struct pwdb));
	ent->passwd = calloc(1, sizeof(struct passwd));

	ent->passwd->pw_name   = strdup(passwd->pw_name);
	ent->passwd->pw_passwd = strdup(passwd->pw_passwd);
	ent->passwd->pw_uid    = passwd->pw_uid;
	ent->passwd->pw_gid    = passwd->pw_gid;
	ent->passwd->pw_gecos  = strdup(passwd->pw_gecos);
	ent->passwd->pw_dir    = strdup(passwd->pw_dir);
	ent->passwd->pw_shell  = strdup(passwd->pw_shell);
	return ent;
}

static pn_word cwa_pwdb_open(pn_machine *m)
{
	struct pwdb *cur, *entry;
	FILE *input;

	input = fopen((const char *)m->A, "r");
	if (!input) return 1;

	UDATA(m)->pwdb = cur = entry = NULL;
	errno = 0;
	while ((entry = s_fgetpwent(input)) != NULL) {
		if (!UDATA(m)->pwdb) {
			UDATA(m)->pwdb = entry;
		} else {
			cur->next = entry;
		}
		cur = entry;
	}

	fclose(input);
	return 0;
}

static pn_word cwa_pwdb_lookup(pn_machine *m)
{
	struct pwdb *db;
	if (m->A) {
		for (db = UDATA(m)->pwdb; db; db = db->next) {
			if (db->passwd && strcmp(db->passwd->pw_name, (const char *)m->B) == 0) {
				UDATA(m)->pwent = db->passwd;
				return 0;
			}
		}
	} else {
		for (db = UDATA(m)->pwdb; db; db = db->next) {
			if (db->passwd && db->passwd->pw_uid == (uid_t)m->B) {
				UDATA(m)->pwent = db->passwd;
				return 0;
			}
		}
	}
	return 1;
}

static pn_word cwa_pwdb_get_uid(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_uid);
}

static pn_word cwa_pwdb_get_gid(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_gid);
}

static pn_word cwa_pwdb_get_name(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_name);
}

static pn_word cwa_pwdb_get_passwd(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_passwd);
}

static pn_word cwa_pwdb_get_gecos(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_gecos);
}

static pn_word cwa_pwdb_get_home(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_dir);
}

static pn_word cwa_pwdb_get_shell(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_shell);
}

static pn_word cwa_pwdb_set_uid(pn_machine *m)
{
	UDATA(m)->pwent->pw_uid = (uid_t)m->B;
	return 0;
}

static pn_word cwa_pwdb_set_gid(pn_machine *m)
{
	UDATA(m)->pwent->pw_gid = (gid_t)m->B;
	return 0;
}

static pn_word cwa_pwdb_set_name(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_name);
	UDATA(m)->pwent->pw_name = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_pwdb_set_passwd(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_passwd);
	UDATA(m)->pwent->pw_passwd = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_pwdb_set_gecos(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_gecos);
	UDATA(m)->pwent->pw_gecos = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_pwdb_set_home(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_dir);
	UDATA(m)->pwent->pw_dir = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_pwdb_set_shell(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_shell);
	UDATA(m)->pwent->pw_shell = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_pwdb_create(pn_machine *m)
{
	if (!UDATA(m)->pwdb) return 1;

	struct pwdb *ent;
	for (ent = UDATA(m)->pwdb; ent->next; ent = ent->next)
		;

	ent->next = calloc(1, sizeof(struct pwdb));
	ent->next->passwd = calloc(1, sizeof(struct passwd));
	ent->next->passwd->pw_name  = strdup((const char *)m->A);
	ent->next->passwd->pw_uid   = (uid_t)m->B;
	ent->next->passwd->pw_gid   = (uid_t)m->C;
	ent->next->passwd->pw_gecos = strdup((const char *)m->A);
	ent->next->passwd->pw_dir   = strdup("/");
	ent->next->passwd->pw_shell = strdup("/bin/false");
	return 0;
}

static pn_word cwa_pwdb_next_uid(pn_machine *m)
{
	if (!UDATA(m)->pwdb) return 1;

	struct pwdb *db;
	uid_t uid = (uid_t)m->A;
UID: while (uid < 65536) {
		for (db = UDATA(m)->pwdb; db; db = db->next) {
			if (db->passwd->pw_uid != uid) continue;
			uid++; goto UID;
		}
		m->B = (pn_word)uid;
		return 0;
	}
	return 1;
}

static pn_word cwa_pwdb_remove(pn_machine *m)
{
	if (!UDATA(m)->pwdb || !UDATA(m)->pwent) return 1;

	struct pwdb *db, *ent = NULL;
	for (db = UDATA(m)->pwdb; db; ent = db, db = db->next) {
		if (db->passwd == UDATA(m)->pwent) {
			free(db->passwd->pw_name);
			free(db->passwd->pw_passwd);
			free(db->passwd->pw_gecos);
			free(db->passwd->pw_dir);
			free(db->passwd->pw_shell);
			free(db->passwd);

			if (ent) {
				ent->next = db->next;
				free(db);
			}
			return 0;
		}
	}
	return 1;
}

static pn_word cwa_pwdb_write(pn_machine *m)
{
	FILE *output;

	output = fopen((const char *)m->A, "w");
	if (!output) return 1;

	struct pwdb *db;
	for (db = UDATA(m)->pwdb; db; db = db->next) {
		if (db->passwd && putpwent(db->passwd, output) == -1) {
			fclose(output);
			return -1;
		}
	}
	fclose(output);
	return 0;
}

static pn_word cwa_pwdb_close(pn_machine *m)
{
	struct pwdb *cur, *entry = UDATA(m)->pwdb;

	while (entry) {
		cur = entry->next;
		free(entry->passwd->pw_name);
		free(entry->passwd->pw_passwd);
		free(entry->passwd->pw_gecos);
		free(entry->passwd->pw_dir);
		free(entry->passwd->pw_shell);
		free(entry->passwd);
		free(entry);
		entry = cur;
	}
	UDATA(m)->pwdb = NULL;
	return 0;
}

/***************************************************/

int pendulum_funcs(pn_machine *m)
{
	pn_func(m, "FS.EXISTS?",   cwa_fs_exists);
	pn_func(m, "FS.FILE?",     cwa_fs_is_file);
	pn_func(m, "FS.DIR?",      cwa_fs_is_dir);
	pn_func(m, "FS.CHARDEV?",  cwa_fs_is_chardev);
	pn_func(m, "FS.BLOCKDEV?", cwa_fs_is_blockdev);
	pn_func(m, "FS.FIFO?",     cwa_fs_is_fifo);
	pn_func(m, "FS.SYMLINK?",  cwa_fs_is_symlink);
	pn_func(m, "FS.SOCKET?",   cwa_fs_is_socket);

	pn_func(m, "FS.MKDIR",     cwa_fs_mkdir);
	pn_func(m, "FS.MKFILE",    cwa_fs_mkfile);
	pn_func(m, "FS.SYMLINK",   cwa_fs_symlink);
	pn_func(m, "FS.HARDLINK",  cwa_fs_link);

	pn_func(m, "FS.UNLINK",  cwa_fs_unlink);
	pn_func(m, "FS.RMDIR",   cwa_fs_rmdir);

	pn_func(m, "FS.CHOWN",   cwa_fs_chown);
	pn_func(m, "FS.CHMOD",   cwa_fs_chmod);


	pn_func(m,  "PWDB.OPEN",        cwa_pwdb_open);
	pn_func(m,  "PWDB.FIND",        cwa_pwdb_lookup);

	pn_func(m,  "PWDB.GET_UID",     cwa_pwdb_get_uid);
	pn_func(m,  "PWDB.GET_GID",     cwa_pwdb_get_gid);
	pn_func(m,  "PWDB.GET_NAME",    cwa_pwdb_get_name);
	pn_func(m,  "PWDB.GET_PASSWD",  cwa_pwdb_get_passwd);
	pn_func(m,  "PWDB.GET_GECOS",   cwa_pwdb_get_gecos);
	pn_func(m,  "PWDB.GET_HOME",    cwa_pwdb_get_home);
	pn_func(m,  "PWDB.GET_SHELL",   cwa_pwdb_get_shell);

	pn_func(m,  "PWDB.SET_UID",     cwa_pwdb_set_uid);
	pn_func(m,  "PWDB.SET_GID",     cwa_pwdb_set_gid);
	pn_func(m,  "PWDB.SET_NAME",    cwa_pwdb_set_name);
	pn_func(m,  "PWDB.SET_PASSWD",  cwa_pwdb_set_passwd);
	pn_func(m,  "PWDB.SET_GECOS",   cwa_pwdb_set_gecos);
	pn_func(m,  "PWDB.SET_HOME",    cwa_pwdb_set_home);
	pn_func(m,  "PWDB.SET_SHELL",   cwa_pwdb_set_shell);

	pn_func(m,  "PWDB.CREATE",      cwa_pwdb_create);
	pn_func(m,  "PWDB.NEXT_UID",    cwa_pwdb_next_uid);
	pn_func(m,  "PWDB.REMOVE",      cwa_pwdb_remove);
	pn_func(m,  "PWDB.WRITE",       cwa_pwdb_write);
	pn_func(m,  "PWDB.CLOSE",       cwa_pwdb_close);

	m->U = calloc(1, sizeof(udata));

	return 0;
}
