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
struct spdb {
	struct spdb *next;       /* the next database entry */
	struct spwd *spwd;       /* shadow account record */
};
struct grdb {
	struct grdb  *next;      /* the next database entry */
	struct group *group;     /* group account record */
};
struct sgdb {
	struct sgdb *next;       /* the next database entry */
	struct sgrp *sgrp;       /* shadow account record */
};

typedef struct {
	struct pwdb   *pwdb;
	struct passwd *pwent;

	struct spdb   *spdb;
	struct spwd   *spent;

	struct grdb   *grdb;
	struct group  *grent;

	struct sgdb   *sgdb;
	struct sgrp   *sgent;
} udata;

#define UDATA(m) ((udata*)(m->U))

/*

    ########  ######
    ##       ##    ##
    ##       ##
    ######    ######
    ##             ##
    ##       ##    ##
    ##        ######

 */

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

/*

    ########  ##      ## ########  ########
    ##     ## ##  ##  ## ##     ## ##     ##
    ##     ## ##  ##  ## ##     ## ##     ##
    ########  ##  ##  ## ##     ## ########
    ##        ##  ##  ## ##     ## ##     ##
    ##        ##  ##  ## ##     ## ##     ##
    ##         ###  ###  ########  ########

 */

static pn_word cwa_pwdb_open(pn_machine *m)
{
	struct pwdb *cur, *ent;
	struct passwd *passwd;
	FILE *input;

	input = fopen((const char *)m->A, "r");
	if (!input) return 1;

	UDATA(m)->pwdb = cur = ent = NULL;
	errno = 0;
	while ((passwd = fgetpwent(input)) != NULL) {
		ent = calloc(1, sizeof(struct pwdb));
		ent->passwd = calloc(1, sizeof(struct passwd));

		ent->passwd->pw_name   = strdup(passwd->pw_name);
		ent->passwd->pw_passwd = strdup(passwd->pw_passwd);
		ent->passwd->pw_uid    = passwd->pw_uid;
		ent->passwd->pw_gid    = passwd->pw_gid;
		ent->passwd->pw_gecos  = strdup(passwd->pw_gecos);
		ent->passwd->pw_dir    = strdup(passwd->pw_dir);
		ent->passwd->pw_shell  = strdup(passwd->pw_shell);

		if (!UDATA(m)->pwdb) {
			UDATA(m)->pwdb = ent;
		} else {
			cur->next = ent;
		}
		cur = ent;
	}

	fclose(input);
	return 0;
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
			return 1;
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

/*

     ######  ########  ########  ########
    ##    ## ##     ## ##     ## ##     ##
    ##       ##     ## ##     ## ##     ##
     ######  ########  ##     ## ########
          ## ##        ##     ## ##     ##
    ##    ## ##        ##     ## ##     ##
     ######  ##        ########  ########

 */

static pn_word cwa_spdb_open(pn_machine *m)
{
	struct spdb *cur, *ent;
	struct spwd *spwd;
	FILE *input;

	input = fopen((const char *)m->A, "r");
	if (!input) return 1;

	UDATA(m)->spdb = cur = ent = NULL;
	errno = 0;
	while ((spwd = fgetspent(input)) != NULL) {
		ent = calloc(1, sizeof(struct spdb));
		ent->spwd = calloc(1, sizeof(struct spwd));

		ent->spwd->sp_namp   = strdup(spwd->sp_namp);
		ent->spwd->sp_pwdp   = strdup(spwd->sp_pwdp);
		ent->spwd->sp_lstchg = spwd->sp_lstchg;
		ent->spwd->sp_min    = spwd->sp_min;
		ent->spwd->sp_max    = spwd->sp_max;
		ent->spwd->sp_warn   = spwd->sp_warn;
		ent->spwd->sp_inact  = spwd->sp_inact;
		ent->spwd->sp_expire = spwd->sp_expire;
		ent->spwd->sp_flag   = spwd->sp_flag;

		if (!UDATA(m)->spdb) {
			UDATA(m)->spdb = ent;
		} else {
			cur->next = ent;
		}
		cur = ent;
	}

	fclose(input);
	return 0;
}

static pn_word cwa_spdb_write(pn_machine *m)
{
	FILE *output;

	output = fopen((const char *)m->A, "w");
	if (!output) return 1;

	struct spdb *db;
	for (db = UDATA(m)->spdb; db; db = db->next) {
		if (db->spwd && putspent(db->spwd, output) == -1) {
			fclose(output);
			return 1;
		}
	}
	fclose(output);
	return 0;
}

static pn_word cwa_spdb_close(pn_machine *m)
{
	struct spdb *cur, *entry = UDATA(m)->spdb;

	while (entry) {
		cur = entry->next;
		free(entry->spwd->sp_namp);
		free(entry->spwd->sp_pwdp);
		free(entry->spwd);
		free(entry);
		entry = cur;
	}
	UDATA(m)->spdb = NULL;
	return 0;
}

/*

     ######   ########  ########  ########
    ##    ##  ##     ## ##     ## ##     ##
    ##        ##     ## ##     ## ##     ##
    ##   #### ########  ##     ## ########
    ##    ##  ##   ##   ##     ## ##     ##
    ##    ##  ##    ##  ##     ## ##     ##
     ######   ##     ## ########  ########

 */

static pn_word cwa_grdb_open(pn_machine *m)
{
	return 0;
}

static pn_word cwa_grdb_write(pn_machine *m)
{
	return 0;
}

static pn_word cwa_grdb_close(pn_machine *m)
{
	return 0;
}

/*

     ######   ######   ########  ########
    ##    ## ##    ##  ##     ## ##     ##
    ##       ##        ##     ## ##     ##
     ######  ##   #### ##     ## ########
          ## ##    ##  ##     ## ##     ##
    ##    ## ##    ##  ##     ## ##     ##
     ######   ######   ########  ########

 */

static pn_word cwa_sgdb_open(pn_machine *m)
{
	return 0;
}

static pn_word cwa_sgdb_write(pn_machine *m)
{
	return 0;
}

static pn_word cwa_sgdb_close(pn_machine *m)
{
	return 0;
}

/*

    ##     ##  ######  ######## ########
    ##     ## ##    ## ##       ##     ##
    ##     ## ##       ##       ##     ##
    ##     ##  ######  ######   ########
    ##     ##       ## ##       ##   ##
    ##     ## ##    ## ##       ##    ##
     #######   ######  ######## ##     ##

 */

static pn_word cwa_user_find(pn_machine *m)
{
	struct pwdb *pw;
	struct spdb *sp;

	if (!UDATA(m)->pwdb) pn_die(m, "passwd database not opened (use PWDB.OPEN)");
	if (!UDATA(m)->spdb) pn_die(m, "shadow database not opened (use SPDB.OPEN)");

	if (m->A) {
		for (pw = UDATA(m)->pwdb; pw; pw = pw->next) {
			if (pw->passwd && strcmp(pw->passwd->pw_name, (const char *)m->B) == 0) {
				UDATA(m)->pwent = pw->passwd;
				goto found;
			}
		}
	} else {
		for (pw = UDATA(m)->pwdb; pw; pw = pw->next) {
			if (pw->passwd && pw->passwd->pw_uid == (uid_t)m->B) {
				UDATA(m)->pwent = pw->passwd;
				goto found;
			}
		}
	}
	return 1;

found:
	/* now, look for the shadow entry */
	for (sp = UDATA(m)->spdb; sp; sp = sp->next) {
		if (sp->spwd && strcmp(sp->spwd->sp_namp, pw->passwd->pw_name) == 0) {
			UDATA(m)->spent = sp->spwd;
			return 0;
		}
	}
	return 1;
}

static pn_word cwa_user_is_locked(pn_machine *m)
{
	return 1;
}

static pn_word cwa_user_get_uid(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_uid);
}

static pn_word cwa_user_get_gid(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_gid);
}

static pn_word cwa_user_get_name(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_name);
}

static pn_word cwa_user_get_passwd(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_passwd);
}

static pn_word cwa_user_get_gecos(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_gecos);
}

static pn_word cwa_user_get_home(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_dir);
}

static pn_word cwa_user_get_shell(pn_machine *m)
{
	return (pn_word)(UDATA(m)->pwent->pw_shell);
}

static pn_word cwa_user_get_pwhash(pn_machine *m)
{
	return (pn_word)(UDATA(m)->spent->sp_pwdp);
}

static pn_word cwa_user_get_pwmin(pn_machine *m)
{
	return (pn_word)(UDATA(m)->spent->sp_min);
}

static pn_word cwa_user_get_pwmax(pn_machine *m)
{
	return (pn_word)(UDATA(m)->spent->sp_max);
}

static pn_word cwa_user_get_pwwarn(pn_machine *m)
{
	return (pn_word)(UDATA(m)->spent->sp_warn);
}

static pn_word cwa_user_get_inact(pn_machine *m)
{
	return (pn_word)(UDATA(m)->spent->sp_inact);
}

static pn_word cwa_user_get_expiry(pn_machine *m)
{
	return (pn_word)(UDATA(m)->spent->sp_expire);
}

static pn_word cwa_user_set_uid(pn_machine *m)
{
	UDATA(m)->pwent->pw_uid = (uid_t)m->B;
	return 0;
}

static pn_word cwa_user_set_gid(pn_machine *m)
{
	UDATA(m)->pwent->pw_gid = (gid_t)m->B;
	return 0;
}

static pn_word cwa_user_set_name(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_name);
	UDATA(m)->pwent->pw_name = strdup((const char *)m->B);

	free(UDATA(m)->spent->sp_namp);
	UDATA(m)->spent->sp_namp = strdup((const char *)m->B);

	return 0;
}

static pn_word cwa_user_set_passwd(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_passwd);
	UDATA(m)->pwent->pw_passwd = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_user_set_gecos(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_gecos);
	UDATA(m)->pwent->pw_gecos = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_user_set_home(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_dir);
	UDATA(m)->pwent->pw_dir = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_user_set_shell(pn_machine *m)
{
	free(UDATA(m)->pwent->pw_shell);
	UDATA(m)->pwent->pw_shell = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_user_set_pwhash(pn_machine *m)
{
	free(UDATA(m)->spent->sp_pwdp);
	UDATA(m)->spent->sp_pwdp = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_user_set_pwmin(pn_machine *m)
{
	UDATA(m)->spent->sp_min = (signed long)(m->A);
	return 0;
}

static pn_word cwa_user_set_pwmax(pn_machine *m)
{
	UDATA(m)->spent->sp_max = (signed long)(m->A);
	return 0;
}

static pn_word cwa_user_set_pwwarn(pn_machine *m)
{
	UDATA(m)->spent->sp_warn = (signed long)(m->A);
	return 0;
}

static pn_word cwa_user_set_inact(pn_machine *m)
{
	UDATA(m)->spent->sp_inact = (signed long)(m->A);
	return 0;
}

static pn_word cwa_user_set_expiry(pn_machine *m)
{
	UDATA(m)->spent->sp_expire = (signed long)(m->A);
	return 0;
}

static pn_word cwa_user_create(pn_machine *m)
{
	if (!UDATA(m)->pwdb) return 1;

	struct pwdb *pw;
	for (pw = UDATA(m)->pwdb; pw->next; pw = pw->next)
		;

	pw->next = calloc(1, sizeof(struct pwdb));
	pw->next->passwd = calloc(1, sizeof(struct passwd));
	pw->next->passwd->pw_name  = strdup((const char *)m->A);
	pw->next->passwd->pw_uid   = (uid_t)m->B;
	pw->next->passwd->pw_gid   = (uid_t)m->C;
	pw->next->passwd->pw_gecos = strdup((const char *)m->A);
	pw->next->passwd->pw_dir   = strdup("/");
	pw->next->passwd->pw_shell = strdup("/bin/false");
	UDATA(m)->pwent = pw->next->passwd;

	struct spdb *sp;
	for (sp = UDATA(m)->spdb; sp->next; sp = sp->next)
		;

	sp->next = calloc(1, sizeof(struct spdb));
	sp->next->spwd = calloc(1, sizeof(struct spwd));
	sp->next->spwd->sp_namp   = strdup((const char *)m->A);
	sp->next->spwd->sp_pwdp   = strdup("*");
	/* FIXME - set sp_lstchg! */
	sp->next->spwd->sp_min    = 0;
	sp->next->spwd->sp_max    = 99999;
	sp->next->spwd->sp_warn   = 7;
	sp->next->spwd->sp_inact  = 0;
	sp->next->spwd->sp_expire = 0;
	UDATA(m)->spent = sp->next->spwd;

	return 0;
}

static pn_word cwa_user_next_uid(pn_machine *m)
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

static pn_word cwa_user_remove(pn_machine *m)
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

/*

    ########  ##     ## ##    ## ######## #### ##     ## ########
    ##     ## ##     ## ###   ##    ##     ##  ###   ### ##
    ##     ## ##     ## ####  ##    ##     ##  #### #### ##
    ########  ##     ## ## ## ##    ##     ##  ## ### ## ######
    ##   ##   ##     ## ##  ####    ##     ##  ##     ## ##
    ##    ##  ##     ## ##   ###    ##     ##  ##     ## ##
    ##     ##  #######  ##    ##    ##    #### ##     ## ########

 */


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
	pn_func(m,  "PWDB.WRITE",       cwa_pwdb_write);
	pn_func(m,  "PWDB.CLOSE",       cwa_pwdb_close);

	pn_func(m,  "SPDB.OPEN",        cwa_spdb_open);
	pn_func(m,  "SPDB.WRITE",       cwa_spdb_write);
	pn_func(m,  "SPDB.CLOSE",       cwa_spdb_close);

	pn_func(m,  "GRDB.OPEN",        cwa_grdb_open);
	pn_func(m,  "GRDB.WRITE",       cwa_grdb_write);
	pn_func(m,  "GRDB.CLOSE",       cwa_grdb_close);

	pn_func(m,  "SGDB.OPEN",        cwa_sgdb_open);
	pn_func(m,  "SGDB.WRITE",       cwa_sgdb_write);
	pn_func(m,  "SGDB.CLOSE",       cwa_sgdb_close);

	pn_func(m,  "USER.FIND",        cwa_user_find);
	pn_func(m,  "USER.NEXT_UID",    cwa_user_next_uid);
	pn_func(m,  "USER.CREATE",      cwa_user_create);
	pn_func(m,  "USER.REMOVE",      cwa_user_remove);

	pn_func(m,  "USER.LOCKED?",     cwa_user_is_locked);

	pn_func(m,  "USER.GET_UID",     cwa_user_get_uid);
	pn_func(m,  "USER.GET_GID",     cwa_user_get_gid);
	pn_func(m,  "USER.GET_NAME",    cwa_user_get_name);
	pn_func(m,  "USER.GET_PASSWD",  cwa_user_get_passwd);
	pn_func(m,  "USER.GET_GECOS",   cwa_user_get_gecos);
	pn_func(m,  "USER.GET_HOME",    cwa_user_get_home);
	pn_func(m,  "USER.GET_SHELL",   cwa_user_get_shell);
	pn_func(m,  "USER.GET_PWHASH",  cwa_user_get_pwhash);
	pn_func(m,  "USER.GET_PWMIN",   cwa_user_get_pwmin);
	pn_func(m,  "USER.GET_PWMAX",   cwa_user_get_pwmax);
	pn_func(m,  "USER.GET_PWWARN",  cwa_user_get_pwwarn);
	pn_func(m,  "USER.GET_INACT",   cwa_user_get_inact);
	pn_func(m,  "USER.GET_EXPIRY",  cwa_user_get_expiry);

	pn_func(m,  "USER.SET_UID",     cwa_user_set_uid);
	pn_func(m,  "USER.SET_GID",     cwa_user_set_gid);
	pn_func(m,  "USER.SET_NAME",    cwa_user_set_name);
	pn_func(m,  "USER.SET_PASSWD",  cwa_user_set_passwd);
	pn_func(m,  "USER.SET_GECOS",   cwa_user_set_gecos);
	pn_func(m,  "USER.SET_HOME",    cwa_user_set_home);
	pn_func(m,  "USER.SET_SHELL",   cwa_user_set_shell);
	pn_func(m,  "USER.SET_PWHASH",  cwa_user_set_pwhash);
	pn_func(m,  "USER.SET_PWMIN",   cwa_user_set_pwmin);
	pn_func(m,  "USER.SET_PWMAX",   cwa_user_set_pwmax);
	pn_func(m,  "USER.SET_PWWARN",  cwa_user_set_pwwarn);
	pn_func(m,  "USER.SET_INACT",   cwa_user_set_inact);
	pn_func(m,  "USER.SET_EXPIRY",  cwa_user_set_expiry);

	m->U = calloc(1, sizeof(udata));

	return 0;
}
