#define _GNU_SOURCE

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
#include <sys/wait.h>
#include <sys/select.h>
#include <fcntl.h>

#include <augeas.h>

#include <pwd.h>
#include <shadow.h>
#include <grp.h>
#include <fts.h>

#include "pendulum_funcs.h"
#include "clockwork.h"

#define CHANGE_FLAG ":changed"

/*

    ##     ## ######## ##       ########  ######## ########   ######
    ##     ## ##       ##       ##     ## ##       ##     ## ##    ##
    ##     ## ##       ##       ##     ## ##       ##     ## ##
    ######### ######   ##       ########  ######   ########   ######
    ##     ## ##       ##       ##        ##       ##   ##         ##
    ##     ## ##       ##       ##        ##       ##    ##  ##    ##
    ##     ## ######## ######## ##        ######## ##     ##  ######

 */

static char *s_format(const char *fmt, ...)
{
	char buf[256];
	char *buf2;
	size_t n;
	va_list args;

	va_start(args, fmt);
	n = vsnprintf(buf, 256, fmt, args) + 1;
	va_end(args);
	if (n > 256) {
		buf2 = calloc(n, sizeof(char));
		if (!buf2) { return NULL; }

		va_start(args, fmt);
		vsnprintf(buf2, n, fmt, args);
		va_end(args);
	} else {
		buf2 = strdup(buf);
	}

	return buf2;
}

static char ** s_strlist_munge(char **l, const char *add, const char *rm)
{
	char **n, **t;
	size_t plus = 1;
	if (add) plus++;
	if (rm)  plus--;

	if (!l) return NULL;
	for (t = l; *t; t++)
		;

	n = calloc(t - l  + plus, sizeof(char*));
	for (t = n; *l; l++) {
		if (rm && strcmp(*l, rm) == 0) continue;
		*t++ = strdup(*l);
	}
	if (add) *t++ = strdup(add);

	return n;
}

void s_strlist_free(char **a)
{
	char **s;
	for (s = a; s && *s; free(*s++));
	free(a);
}

static char ** s_strlist_dup(char **l)
{
	return s_strlist_munge(l, NULL, NULL);
}

static char ** s_strlist_add(char **l, const char *add)
{
	char **n = s_strlist_munge(l, add, NULL);
	if (n) s_strlist_free(l);
	return n;
}

static char ** s_strlist_remove(char **l, const char *rm)
{
	char **n = s_strlist_munge(l, NULL, rm);
	if (n) s_strlist_free(l);
	return n;
}

static char * s_strlist_join(char **l, const char *delim)
{
	size_t n = 1; /* null terminator */
	size_t dn = strlen(delim);
	char **i;
	for (i = l; *i; n += strlen(*i) + (i == l ? 0 : dn), i++)
		;

	char *p, *buf = calloc(n, sizeof(char));
	for (p = buf, i = l; *i; i++) {
		if (i != l) {
			strcpy(p, delim);
			p += dn;
		}
		strcpy(p, *i);
		p += strlen(*i);
	}
	return buf;
}

/*

     ######   #######  ##     ## ########     ###    ########
    ##    ## ##     ## ###   ### ##     ##   ## ##      ##
    ##       ##     ## #### #### ##     ##  ##   ##     ##
    ##       ##     ## ## ### ## ########  ##     ##    ##
    ##       ##     ## ##     ## ##        #########    ##
    ##    ## ##     ## ##     ## ##        ##     ##    ##
     ######   #######  ##     ## ##        ##     ##    ##

 */

/* for older versions of glibc */
/* ripped from some version of glibc, >2.5 */
struct sgrp {
	char  *sg_namp;
	char  *sg_passwd;
	char **sg_adm;
	char **sg_mem;
};
static char SGBUF[8192], *SGADM[128], *SGMEM[128];
static struct sgrp SGENT;
static struct sgrp* fgetsgent(FILE *io)
{
	char *a, *b, *c;
	if (feof(io) || !fgets(SGBUF, 8191, io)) { return NULL; }

	SGENT.sg_mem = SGMEM;
	SGENT.sg_adm = SGADM;

	/* name */
	for (a = SGBUF, b = a; *b && *b != ':'; b++)
		;
	*b = '\0';
	SGENT.sg_namp = a;

	/* password */
	for (a = ++b; *b && *b != ':'; b++)
		;
	*b = '\0';
	SGENT.sg_passwd = a;

	/* admins */
	for (a = ++b; *b && *b != ':'; b++)
		;
	*b = '\0'; c = a;
	while (c < b) {
		for (a = c; *c && *c != ','; c++)
			;
		*c++ = '\0';
		*(SGENT.sg_adm++) = a;
	};
	*(SGENT.sg_adm) = NULL;

	/* members */
	for (a = ++b; *b && *b != '\n'; b++)
		;
	*b = '\0'; c = a;
	while (c < b) {
		for (a = c; *c && *c != ','; c++)
			;
		*c++ = '\0';
		*(SGENT.sg_mem++) = a;
	};
	*(SGENT.sg_mem) = NULL;

	SGENT.sg_mem = SGMEM;
	SGENT.sg_adm = SGADM;

	return &SGENT;
}

static int putsgent(const struct sgrp *g, FILE *io)
{
	char *members, *admins;
	int ret;

	admins  = s_strlist_join(g->sg_adm, ",");
	members = s_strlist_join(g->sg_mem, ",");

	ret = fprintf(io, "%s:%s:%s:%s\n", g->sg_namp, g->sg_passwd, admins, members);

	free(admins);
	free(members);

	return ret;
}

/*

    ########     ###    ########    ###
    ##     ##   ## ##      ##      ## ##
    ##     ##  ##   ##     ##     ##   ##
    ##     ## ##     ##    ##    ##     ##
    ##     ## #########    ##    #########
    ##     ## ##     ##    ##    ##     ##
    ########  ##     ##    ##    ##     ##

 */

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
	void          *zconn;
	const char    *fsroot;

	char etc_passwd[1024];
	struct pwdb   *pwdb;
	struct passwd *pwent;

	char etc_shadow[1024];
	struct spdb   *spdb;
	struct spwd   *spent;

	char etc_group[1024];
	struct grdb   *grdb;
	struct group  *grent;

	char etc_gshadow[1024];
	struct sgdb   *sgdb;
	struct sgrp   *sgent;

	augeas        *aug_ctx;
	char          *aug_last;

	struct SHA1    lsha1;

	char *difftool;
} udata;

#define UDATA(m) ((udata*)(m->U))

/*

    ########  ########     ###     ######   ##     ##    ###
    ##     ## ##     ##   ## ##   ##    ##  ###   ###   ## ##
    ##     ## ##     ##  ##   ##  ##        #### ####  ##   ##
    ########  ########  ##     ## ##   #### ## ### ## ##     ##
    ##        ##   ##   ######### ##    ##  ##     ## #########
    ##        ##    ##  ##     ## ##    ##  ##     ## ##     ##
    ##        ##     ## ##     ##  ######   ##     ## ##     ##

 */

static pn_word uf_pragma(pn_machine *m, const char *k, const char *v)
{
	pn_trace(m, "PRAGMA '%s'\n", k);
	if (strcmp(k, "userdb.root") == 0) {
		size_t n;
		n = snprintf(UDATA(m)->etc_passwd,  1023, "%s%s", v, "/etc/passwd");
		UDATA(m)->etc_passwd[n] = '\0';
		pn_trace(m, " - setting passwd db path to %s\n", UDATA(m)->etc_passwd);

		n = snprintf(UDATA(m)->etc_shadow,  1023, "%s%s", v, "/etc/shadow");
		UDATA(m)->etc_shadow[n] = '\0';
		pn_trace(m, " - setting shadow db path to %s\n", UDATA(m)->etc_shadow);

		n = snprintf(UDATA(m)->etc_group,   1023, "%s%s", v, "/etc/group");
		UDATA(m)->etc_group[n] = '\0';
		pn_trace(m, " - setting group db path to %s\n", UDATA(m)->etc_group);

		n = snprintf(UDATA(m)->etc_gshadow, 1023, "%s%s", v, "/etc/gshadow");
		UDATA(m)->etc_gshadow[n] = '\0';
		pn_trace(m, " - setting gshadow db path to %s\n", UDATA(m)->etc_gshadow);
		return 0;
	}

	if (strcmp(k, "diff.tool") == 0) {
		free(UDATA(m)->difftool);
		UDATA(m)->difftool = strdup(v);
		pn_trace(m, " - setting diff tool to '%s'", UDATA(m)->difftool);
		return 0;
	}

	pn_trace(m, " !!! '%s' is an unknown PRAGMA", k);
	return 1;
}

/*

    ########  ######
    ##       ##    ##
    ##       ##
    ######    ######
    ##             ##
    ##       ##    ##
    ##        ######

 */

static void s_mkdir_parents(pn_machine *m, const char *spath)
{
	struct path *p;
	p = path_new(spath);
	if (!p || path_canon(p) != 0) goto failed;

	struct stat st;
	int rc = 1;

	/* find the first parent dir */
	while (path_pop(p) != 0) {
		rc = stat(path(p), &st);
		if (rc == 0)
			break;
	}
	if (rc != 0) goto failed;

	/* create missing parents, with ownership/mode of
	   the first pre-existing parent dir */
	for (path_push(p); strcmp(path(p), spath) != 0; path_push(p)) {
		cw_log(LOG_NOTICE, "%s creating parent directory %s, owned by %i:%i, mode %#4o",
			m->topic, path(p), st.st_uid, st.st_gid, st.st_mode);

		rc = mkdir(path(p), st.st_mode);
		if (rc != 0) break;

		rc = chown(path(p), st.st_uid, st.st_gid);
		if (!rc) continue;
	}

failed:
	path_free(p);
	return;
}

static pn_word uf_fs_is_file(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.IS_FILE? '%s'\n", (const char *)m->A);
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISREG(st.st_mode) ? 0 : 1;
}

static pn_word uf_fs_is_dir(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.IS_DIR? '%s'\n", (const char *)m->A);
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISDIR(st.st_mode) ? 0 : 1;
}

static pn_word uf_fs_is_chardev(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.IS_CHARDEV? '%s'\n", (const char *)m->A);
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISCHR(st.st_mode) ? 0 : 1;
}

static pn_word uf_fs_is_blockdev(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.IS_BLOCKDEV? '%s'\n", (const char *)m->A);
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISBLK(st.st_mode) ? 0 : 1;
}

static pn_word uf_fs_is_fifo(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.IS_FIFO? '%s'\n", (const char *)m->A);
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISFIFO(st.st_mode) ? 0 : 1;
}

static pn_word uf_fs_is_symlink(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.IS_SYMLINK? '%s'\n", (const char *)m->A);
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISLNK(st.st_mode) ? 0 : 1;
}

static pn_word uf_fs_is_socket(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.IS_SOCKET? '%s'\n", (const char *)m->A);
	if (lstat((const char *)m->A, &st) != 0) return 1;
	return S_ISSOCK(st.st_mode) ? 0 : 1;
}

static pn_word uf_fs_sha1(pn_machine *m)
{
	pn_trace(m, "FS.SHA1 '%s'\n", (const char *)m->A);
	int rc = sha1_file((const char *)m->A, &(UDATA(m)->lsha1));
	m->S2 = (pn_word)(UDATA(m)->lsha1.hex);
	return rc;
}

static pn_word uf_fs_exists(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "&FS.EXISTS? '%s'\n", (const char *)m->A);
	return lstat((const char *)m->A, &st) == 0 ? 0 : 1;
}

static pn_word uf_fs_mkdir(pn_machine *m)
{
	pn_trace(m, "FS.MKDIR '%s' <0777>\n", (const char *)m->A);
	if (uf_fs_exists(m) == 0) return 1;
	pn_flag(m, CHANGE_FLAG, 1);
	s_mkdir_parents(m, (const char *)m->A);
	cw_log(LOG_NOTICE, "%s creating directory %s", m->topic, (const char *)m->A);
	return mkdir((const char *)m->A, 0777) == 0 ? 0 : 1;
}

static pn_word uf_fs_mkfile(pn_machine *m)
{
	pn_trace(m, "FS.MKFILE '%s' <0666>\n", (const char *)m->A);
	if (uf_fs_exists(m) == 0) return 1;
	pn_flag(m, CHANGE_FLAG, 1);
	s_mkdir_parents(m, (const char *)m->A);
	cw_log(LOG_NOTICE, "%s creating file %s", m->topic, (const char *)m->A);
	int fd = open((const char *)m->A, O_WRONLY|O_CREAT, 0666);
	if (fd < 0) return 1;
	close(fd);
	return 0;
}

static pn_word uf_fs_symlink(pn_machine *m)
{
	pn_trace(m, "FS.SYMLINK %s -> %s\n", (const char *)m->A, (const char *)m->B);
	if (uf_fs_exists(m) == 0) return 1;
	pn_flag(m, CHANGE_FLAG, 1);
	return symlink((const char *)m->B, (const char *)m->A) == 0 ? 0 : 1;
}

static pn_word uf_fs_link(pn_machine *m)
{
	if (uf_fs_exists(m) == 0) return 1;
	pn_trace(m, "FS.HARDLINK %s -> %s\n", (const char *)m->A, (const char *)m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	return link((const char *)m->B, (const char *)m->A) == 0 ? 0 : 1;
}

static int s_copy_r(const char *dst, const char *src, uid_t uid, gid_t gid)
{
	FTS *fts;
	FTSENT *ent;
	char *path_argv[2] = { strdup(src), NULL };
	char *src_path, *dst_path;
	int src_fd, dst_fd;
	mode_t mode;

	fts = fts_open(path_argv, FTS_PHYSICAL, NULL);
	if (!fts) {
		free(path_argv[0]);
		return -1;
	}

	while ( (ent = fts_read(fts)) != NULL ) {
		if (strcmp(ent->fts_accpath, ent->fts_path) != 0) {
			dst_path = cw_string("%s/%s", dst, ent->fts_accpath);
			src_path = cw_string("%s/%s", src, ent->fts_accpath);
			mode = ent->fts_statp->st_mode & 0777;

			if (S_ISREG(ent->fts_statp->st_mode)) {
				src_fd = open(src_path, O_RDONLY);
				dst_fd = creat(dst_path, mode);

				if (src_fd >= 0 && dst_fd >= 0
				 && fchown(dst_fd, uid, gid) == 0) {
					char buf[8192];
					ssize_t n = 0;

					for (;;) {
						if ((n = read(src_fd, buf, 8192)) <= 0)
							break;
						if (write(dst_fd, buf, n) != n)
							break;
					}
				}
			} else if (S_ISDIR(ent->fts_statp->st_mode)) {
				if (mkdir(dst_path, mode) == 0
				 && chown(dst_path, uid, gid) == 0) {
					chmod(dst_path, 0755);
				}
			}

			free(dst_path);
			free(src_path);
		}
	}

	free(path_argv[0]);
	fts_close(fts);
	return 0;
}
static pn_word uf_fs_copy_r(pn_machine *m)
{
	pn_trace(m, "FS.COPY_R %s -> %s %i:%i\n",
			(const char *)m->A, (const char *)m->B, m->C, m->D);
	return s_copy_r((const char *)m->A, (const char *)m->B,
		(uid_t)m->C, (gid_t)m->D);
}

static pn_word uf_fs_chown(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.CHOWN '%s' %i:%i\n", (const char *)m->A, m->B, m->C);
	if (lstat((const char *)m->A, &st) != 0) return 1;
	if (st.st_uid == (uid_t)m->B && st.st_gid == (gid_t)m->C) return 0;
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set ownership of %s to %i:%i",
		m->topic, (const char *)m->A, m->B, m->C);
	return chown((const char *)m->A, m->B, m->C) == 0 ? 0 : 1;
}

static pn_word uf_fs_chmod(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.CHMOD '%s' %#4o\n", (const char *)m->A, m->D & 04777);
	if (lstat((const char *)m->A, &st) != 0) return 1;
	if ((st.st_mode & 04777) == (m->D & 04777)) return 0;
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set mode of %s to %#4o",
		m->topic, (const char *)m->A, m->D & 04777);
	return chmod((const char *)m->A, (m->D & 04777)) == 0 ? 0 : 1;
}

static pn_word uf_fs_unlink(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.UNLINK %s\n", (const char *)m->A);
	if (lstat((const char *)m->A, &st) != 0) return 0;
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s removing file %s", m->topic, (const char *)m->A);
	return unlink((const char *)m->A) == 0 ? 0 : 1;
}

static pn_word uf_fs_rmdir(pn_machine *m)
{
	struct stat st;
	pn_trace(m, "FS.RMDIR %s\n", (const char *)m->A);
	if (lstat((const char *)m->A, &st) != 0) return 0;
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s removing directory %s", m->topic, (const char *)m->A);
	return rmdir((const char *)m->A) == 0 ? 0 : 1;
}

static pn_word uf_fs_put(pn_machine *m)
{
	pn_trace(m, "FS.PUT '%s' -> %s\n", (const char *)m->B, (const char *)m->A);

	FILE *output = fopen((const char *)m->A, "w");
	if (!output) return 1;

	size_t n = fprintf(output, "%s", (const char *)m->B);
	fclose(output);
	return n == strlen((const char *)m->B) ? 0 : 1;
}

static pn_word uf_fs_get(pn_machine *m)
{
	pn_trace(m, "FS.GET %s\n", (const char *)m->A);

	FILE *input = fopen((const char *)m->A, "r");
	if (!input) return 1;

	char buf[8192] = {0};
	if (!fgets(buf, 8192, input)) {
		fclose(input);
		return 1;
	}
	m->S2 = (pn_word)strdup(buf);
	fclose(input);
	return 0;
}

/*

       ###    ##     ##  ######   ########    ###     ######
      ## ##   ##     ## ##    ##  ##         ## ##   ##    ##
     ##   ##  ##     ## ##        ##        ##   ##  ##
    ##     ## ##     ## ##   #### ######   ##     ##  ######
    ######### ##     ## ##    ##  ##       #########       ##
    ##     ## ##     ## ##    ##  ##       ##     ## ##    ##
    ##     ##  #######   ######   ######## ##     ##  ######

 */

static pn_word uf_aug_init(pn_machine *m)
{
	pn_trace(m, "AUGEAS.INIT '%s' '%s'\n", (const char *)m->A, (const char *)m->B);
	UDATA(m)->aug_ctx = aug_init(
		(const char *)m->A, (const char *)m->B,
		AUG_NO_STDINC|AUG_NO_LOAD|AUG_NO_MODL_AUTOLOAD);

	if (aug_set(UDATA(m)->aug_ctx, "/augeas/load/Hosts/lens", "Hosts.lns") < 0
	 || aug_set(UDATA(m)->aug_ctx, "/augeas/load/Hosts/incl", "/etc/hosts") < 0
	 || aug_load(UDATA(m)->aug_ctx) != 0) {
		return 1;
	}
	return 0;
}

static pn_word uf_aug_syserr(pn_machine *m)
{
	char **err = NULL;
	const char *value;
	int rc = aug_match(UDATA(m)->aug_ctx, "/augeas//error", &err);
	if (!rc) return 1;

	int i;
	fprintf(stderr, "found %u augeas errors\n", rc);
	for (i = 0; i < rc; i++) {
		aug_get(UDATA(m)->aug_ctx, err[i], &value);
		fprintf(stderr, "augeas error: %s - %s\n", err[i], value);
	}
	free(err);
	return 0;
}

static pn_word uf_aug_save(pn_machine *m)
{
	return aug_save(UDATA(m)->aug_ctx);
}

static pn_word uf_aug_close(pn_machine *m)
{
	aug_close(UDATA(m)->aug_ctx);
	UDATA(m)->aug_ctx = NULL;
	return 0;
}

static pn_word uf_aug_set(pn_machine *m)
{
	char *path = s_format((const char *)m->A, m->C, m->D, m->E, m->F);
	pn_trace(m, "AUGEAS.SET %s = '%s'\n", path, (const char *)m->B);
	int rc = aug_set(UDATA(m)->aug_ctx, path, (const char *)m->B);
	free(path);
	return rc;
}

static pn_word uf_aug_get(pn_machine *m)
{
	char *path = s_format((const char *)m->A, m->C, m->D, m->E, m->F);
	pn_trace(m, "AUGEAS.GET %s\n", path);
	int rc = aug_get(UDATA(m)->aug_ctx, path, (const char **)&(UDATA(m)->aug_last));
	m->S2 = (pn_word)(UDATA(m)->aug_last);
	free(path);
	return rc == 1 ? 0 : 1;
}

static pn_word uf_aug_find(pn_machine *m)
{
	char **r = NULL;
	char *path = s_format((const char *)m->A, m->C, m->D, m->E, m->F);
	pn_trace(m, "AUGEAS.FIND %s\n", path);
	int rc = aug_match(UDATA(m)->aug_ctx, path, &r);
	free(path);

	if (rc == 1) {
		UDATA(m)->aug_last = r[0];
		m->S2 = (pn_word)(UDATA(m)->aug_last);
		free(r);
		return 0;
	}
	while (rc > 0)
		free(r[--rc]);
	return 1;
}

static pn_word uf_aug_remove(pn_machine *m)
{
	char *path = s_format((const char *)m->A, m->C, m->D, m->E, m->F);
	pn_trace(m, "AUGEAS.REMOVE %s\n", path);
	int rc = aug_rm(UDATA(m)->aug_ctx, path) > 1 ? 0 : 1;
	free(path);
	return rc;
}

/*

    ##     ## ######## #### ##
    ##     ##    ##     ##  ##
    ##     ##    ##     ##  ##
    ##     ##    ##     ##  ##
    ##     ##    ##     ##  ##
    ##     ##    ##     ##  ##
     #######     ##    #### ########

*/

static pn_word uf_util_vercmp(pn_machine *m)
{
	const char *have = (const char *)m->T1;
	const char *want = (const char *)m->T2;
	pn_trace(m, "UTIL.VERCMP: comparing '%s' %%T1(%p) to '%s' %%T2(%p)'",
		have, have, want, want);
	if (strcmp(have, want) == 0) return 0;
	if (strchr(want, '-')) return 1;
	if (strchr(have, '-') && strncmp(have, want, strchr(have, '-')-have) == 0)
		return 0;
	return 1;
}

/*

    ######## ##     ## ########  ######
    ##        ##   ##  ##       ##    ##
    ##         ## ##   ##       ##
    ######      ###    ######   ##
    ##         ## ##   ##       ##
    ##        ##   ##  ##       ##    ##
    ######## ##     ## ########  ######

*/

static pn_word uf_exec_check(pn_machine *m)
{
	pn_trace(m, "EXEC.CHECK (as %i:%i) `%s`\n", m->B, m->C, (const char *)m->A);
	cw_runner_t runner = {
		.in  = NULL,
		.out = tmpfile(),
		.err = tmpfile(),
		.uid = (uid_t)m->B,
		.gid = (gid_t)m->C
	};
	int rc = cw_run2(&runner, "/bin/sh", "-c", (const char *)m->A, NULL);

	if (cw_logio(LOG_DEBUG, "%s", runner.out) != 0)
		cw_log(LOG_ERR, "Failed to read standard output from `%s`: %s",
			(const char *)m->A, strerror(errno));
	if (cw_logio(LOG_WARNING, "%s", runner.err) != 0)
		cw_log(LOG_ERR, "Failed to read standard error from `%s`: %s",
			(const char *)m->A, strerror(errno));

	cw_log(LOG_DEBUG, "Command `%s` exited %u", (const char *)m->A, rc);
	return rc;
}

static pn_word uf_exec_run1(pn_machine *m)
{
	pn_trace(m, "EXEC.RUN1 (as %i:%i) `%s`\n", m->B, m->C, (const char *)m->A);
	cw_runner_t runner = {
		.in  = NULL,
		.out = tmpfile(),
		.err = tmpfile(),
		.uid = (uid_t)m->B,
		.gid = (gid_t)m->C
	};
	int rc = cw_run2(&runner, "/bin/sh", "-c", (const char *)m->A, NULL);

	char *out = NULL;
	char buf[8192];
	if (fgets(buf, sizeof(buf), runner.out)) {
		char *nl = strchr(buf, '\n');
		if (nl) *nl = '\0';
		out = strdup(buf);
	}

	if (out) cw_log(LOG_DEBUG, "%s", out ? out : "(none)");
	if (cw_logio(LOG_DEBUG, "stdout: %s", runner.out) != 0)
		cw_log(LOG_ERR, "Failed to read standard output from `%s`: %s",
			(const char *)m->A, strerror(errno));
	if (cw_logio(LOG_WARNING, "%s", runner.err) != 0)
		cw_log(LOG_ERR, "Failed to read standard error from `%s`: %s",
			(const char *)m->A, strerror(errno));

	if (!out) out = strdup("");
	cw_log(LOG_DEBUG, "Command `%s` exited %u", (const char *)m->A, rc);
	pn_trace(m, "first line of output was '%s'\n", out);

	m->S2 = (pn_word)out;
	pn_heap_purge(m);
	pn_heap_add(m, out);
	return rc;
}

/*

     ######  ######## ########  ##     ## ######## ########
    ##    ## ##       ##     ## ##     ## ##       ##     ##
    ##       ##       ##     ## ##     ## ##       ##     ##
     ######  ######   ########  ##     ## ######   ########
          ## ##       ##   ##    ##   ##  ##       ##   ##
    ##    ## ##       ##    ##    ## ##   ##       ##    ##
     ######  ######## ##     ##    ###    ######## ##     ##

 */

static pn_word uf_server_sha1(pn_machine *m)
{
	pn_trace(m, "SERVER.SHA1 %s\n", (const char *)m->A);

	cw_pdu_t *pdu, *reply;
	pdu = cw_pdu_make(NULL, 2, "FILE", (const char *)m->A);
	int rc = cw_pdu_send(UDATA(m)->zconn, pdu);
	assert(rc == 0);
	cw_pdu_destroy(pdu);

	reply = cw_pdu_recv(UDATA(m)->zconn);
	if (!reply) {
		cw_log(LOG_ERR, "SERVER.SHA1 failed: %s", zmq_strerror(errno));
		return 1;
	}
	cw_log(LOG_DEBUG, "Received a '%s' PDU", reply->type);
	if (strcmp(reply->type, "ERROR") == 0) {
		char *e = cw_pdu_text(reply, 1);
		cw_log(LOG_ERR, "SERVER.SHA1 protocol violation: %s", e);
		free(e);
		cw_pdu_destroy(reply);
		return 1;
	}

	if (strcmp(reply->type, "SHA1") == 0) {
		char *s = cw_pdu_text(reply, 1);
		pn_heap_purge(m);
		pn_heap_add(m, s);
		m->S2 = (pn_word)s;
		cw_pdu_destroy(reply);
		return 0;
	}

	cw_log(LOG_ERR, "Unexpected PDU received.");
	cw_pdu_destroy(reply);
	return 1;
}

static int s_copyfile(FILE *src, const char *path)
{
	FILE *dst = fopen(path, "w");
	if (!dst) {
		cw_log(LOG_ERR, "copyfile failed: %s", strerror(errno));
		fclose(src);
		return 1;
	}

	rewind(src);
	char buf[8192];
	for (;;) {
		size_t nread = fread(buf, 1, 8192, src);
		if (nread == 0) {
			if (feof(src)) break;
			cw_log(LOG_ERR, "read error: %s", strerror(errno));
			fclose(dst);
			return 1;
		}

		size_t nwritten = fwrite(buf, 1, nread, dst);
		if (nwritten != nread) {
			cw_log(LOG_ERR, "read error: %s", strerror(errno));
			fclose(dst);
			return 1;
		}
	}

	fclose(dst);
	return 0;
}

static pn_word uf_server_writefile(pn_machine *m)
{
	int rc, n = 0;
	char size[16];
	cw_pdu_t *pdu, *reply;

	FILE *tmpf = tmpfile();
	if (!tmpf) {
		cw_log(LOG_ERR, "SERVER.WRITEFILE failed to create temporary file: %s",
				strerror(errno));
		return 1;
	}
	for (;;) {
		rc = snprintf(size, 15, "%i", n++);
		assert(rc > 0);
		pdu = cw_pdu_make(NULL, 2, "DATA", size);
		rc = cw_pdu_send(UDATA(m)->zconn, pdu);
		assert(rc == 0);

		reply = cw_pdu_recv(UDATA(m)->zconn);
		if (!reply) {
			cw_log(LOG_ERR, "SERVER.WRITEFILE failed: %s", zmq_strerror(errno));
			if (tmpf) fclose(tmpf);
			return 1;
		}
		cw_log(LOG_DEBUG, "server.writefile: received a %s PDU", reply->type);

		if (strcmp(reply->type, "EOF") == 0)
			break;
		if (strcmp(reply->type, "BLOCK") != 0) {
			cw_log(LOG_ERR, "protocol violation: received a %s PDU (expected a BLOCK)", reply->type);
			if (tmpf) fclose(tmpf);
			return 1;
		}

		char *data = cw_pdu_text(reply, 1);
		fprintf(tmpf, "%s", data);
		free(data);
	}

	if (UDATA(m)->difftool) {
		rewind(tmpf);
		cw_log(LOG_DEBUG, "Running diff.tool: `%s NEWFILE OLDFILE'", UDATA(m)->difftool);
		FILE *out = tmpfile();
		if (!out) {
			cw_log(LOG_ERR, "Failed to open a temporary file difftool output: %s", strerror(errno));
		} else {
			FILE *err = tmpfile();
			cw_runner_t runner = {
				.in  = tmpf,
				.out = out,
				.err = err,
				.uid = 0,
				.gid = 0,
			};

			char *diffcmd = cw_string("%s %s -", UDATA(m)->difftool, (const char *)m->A);
			rc = cw_run2(&runner, "/bin/sh", "-c", diffcmd, NULL);
			if (rc < 0)
				cw_log(LOG_ERR, "`%s' killed or otherwise terminated abnormally");
			else if (rc == 127)
				cw_log(LOG_ERR, "`%s': command not found");
			else
				cw_log(LOG_DEBUG, "`%s' exited %i", diffcmd, rc);

			if (cw_logio(LOG_INFO, "%s", out) != 0)
				cw_log(LOG_ERR, "Failed to read standard output from difftool `%s': %s", diffcmd, strerror(errno));
			if (cw_logio(LOG_ERR, "diftool: %s", err) != 0)
				cw_log(LOG_ERR, "Failed to read standard error from difftool `%s': %s", diffcmd, strerror(errno));

			fclose(out);
			fclose(err);
			free(diffcmd);
		}
	}
	rc = s_copyfile(tmpf, (const char *)m->A);
	fclose(tmpf);
	pn_flag(m, CHANGE_FLAG, rc == 0 ? 1 : 0);
	return rc == 0 ? 0 : 1;
}


/*

    ##     ##  ######  ######## ########  ########  ########
    ##     ## ##    ## ##       ##     ## ##     ## ##     ##
    ##     ## ##       ##       ##     ## ##     ## ##     ##
    ##     ##  ######  ######   ########  ##     ## ########
    ##     ##       ## ##       ##   ##   ##     ## ##     ##
    ##     ## ##    ## ##       ##    ##  ##     ## ##     ##
     #######   ######  ######## ##     ## ########  ########

 */
static int _pwdb_open(pn_machine *m)
{
	struct pwdb *cur, *ent;
	struct passwd *passwd;
	FILE *input;

	UDATA(m)->pwdb = cur = ent = NULL;

	input = fopen(UDATA(m)->etc_passwd, "r");
	if (!input) return 0;

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
static int _pwdb_write(pn_machine *m)
{
	FILE *output;

	output = fopen(UDATA(m)->etc_passwd, "w");
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
static pn_word _pwdb_close(pn_machine *m)
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
static int _spdb_open(pn_machine *m)
{
	struct spdb *cur, *ent;
	struct spwd *spwd;
	FILE *input;
	UDATA(m)->spdb = cur = ent = NULL;

	input = fopen(UDATA(m)->etc_shadow, "r");
	if (!input) return 0;

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
static int _spdb_write(pn_machine *m)
{
	FILE *output;

	output = fopen(UDATA(m)->etc_shadow, "w");
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
static int _spdb_close(pn_machine *m)
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
static int _grdb_open(pn_machine *m)
{
	struct grdb *cur, *ent;
	struct group *group;
	FILE *input;
	UDATA(m)->grdb = cur = ent = NULL;

	input = fopen(UDATA(m)->etc_group, "r");
	if (!input) return 0;

	errno = 0;

	while ((group = fgetgrent(input)) != NULL) {
		ent = calloc(1, sizeof(struct grdb));
		ent->group = calloc(1, sizeof(struct group));

		ent->group->gr_name   = strdup(group->gr_name);
		ent->group->gr_gid    = group->gr_gid;
		ent->group->gr_passwd = strdup(group->gr_passwd);
		ent->group->gr_mem    = s_strlist_dup(group->gr_mem);

		if (!UDATA(m)->grdb) {
			UDATA(m)->grdb = ent;
		} else {
			cur->next = ent;
		}
		cur = ent;
	}

	fclose(input);
	return 0;
}
static int _grdb_write(pn_machine *m)
{
	FILE *output;

	output = fopen(UDATA(m)->etc_group, "w");
	if (!output) return 1;

	struct grdb *db;
	for (db = UDATA(m)->grdb; db; db = db->next) {
		if (db->group && putgrent(db->group, output) == -1) {
			fclose(output);
			return 1;
		}
	}
	fclose(output);
	return 0;
}
static int _grdb_close(pn_machine *m)
{
	struct grdb *cur;
	struct grdb *ent = UDATA(m)->grdb;

	while (ent) {
		cur = ent->next;
		free(ent->group->gr_name);
		free(ent->group->gr_passwd);
		s_strlist_free(ent->group->gr_mem);
		free(ent->group);
		free(ent);
		ent = cur;
	}
	UDATA(m)->grdb = NULL;
	return 0;
}
static int _sgdb_open(pn_machine *m)
{
	struct sgdb *cur, *ent;
	struct sgrp *sgrp;
	FILE *input;
	UDATA(m)->sgdb = cur = ent = NULL;

	input = fopen(UDATA(m)->etc_gshadow, "r");
	if (!input) return 0;

	errno = 0;
	while ((sgrp = fgetsgent(input)) != NULL) {
		ent = calloc(1, sizeof(struct sgdb));
		ent->sgrp = calloc(1, sizeof(struct sgrp));

		ent->sgrp->sg_namp   = strdup(sgrp->sg_namp);
		ent->sgrp->sg_passwd = strdup(sgrp->sg_passwd);
		ent->sgrp->sg_mem    = s_strlist_dup(sgrp->sg_mem);
		ent->sgrp->sg_adm    = s_strlist_dup(sgrp->sg_adm);

		if (!UDATA(m)->sgdb) {
			UDATA(m)->sgdb = ent;
		} else {
			cur->next = ent;
		}
		cur = ent;
	}

	fclose(input);
	return 0;
}
static int _sgdb_write(pn_machine *m)
{
	FILE *output;

	output = fopen(UDATA(m)->etc_gshadow, "w");
	if (!output) return 1;

	struct sgdb *db;
	for (db = UDATA(m)->sgdb; db; db = db->next) {
		if (db->sgrp && putsgent(db->sgrp, output) == -1) {
			fclose(output);
			return 1;
		}
	}
	fclose(output);
	return 0;
}
static int _sgdb_close(pn_machine *m)
{
	struct sgdb *cur;
	struct sgdb *ent = UDATA(m)->sgdb;

	while (ent) {
		cur = ent->next;
		free(ent->sgrp->sg_namp);
		free(ent->sgrp->sg_passwd);
		s_strlist_free(ent->sgrp->sg_mem);
		s_strlist_free(ent->sgrp->sg_adm);
		free(ent->sgrp);
		free(ent);
		ent = cur;
	}
	UDATA(m)->sgdb = NULL;
	return 0;
}

static pn_word uf_userdb_close(pn_machine *m)
{
	_sgdb_close(m); _grdb_close(m);
	_spdb_close(m); _pwdb_close(m);
	return 0;
}

static pn_word uf_userdb_open(pn_machine *m)
{
	/* in case it wasn't closed from a previous open */
	uf_userdb_close(m);

	if (_pwdb_open(m) == 0 && _spdb_open(m) == 0
	 && _grdb_open(m) == 0 && _sgdb_open(m) == 0) return 0;

	/* open failed */
	uf_userdb_close(m);
	return 1;
}

static pn_word uf_userdb_save(pn_machine *m)
{
	int rc = 1;
	if (_pwdb_write(m) == 0 && _spdb_write(m) == 0
	 && _grdb_write(m) == 0 && _sgdb_write(m) == 0) rc = 0;

	_sgdb_close(m); _grdb_close(m);
	_spdb_close(m); _pwdb_close(m);
	return rc;
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

static pn_word uf_user_find(pn_machine *m)
{
	struct pwdb *pw;
	struct spdb *sp;

	if (!UDATA(m)->pwdb) pn_die(m, "passwd database not opened (use USERDB.OPEN)");
	if (!UDATA(m)->spdb) pn_die(m, "shadow database not opened (use USERDB.OPEN)");

	if (m->A) {
		pn_trace(m, "USER.FIND (by name) %s\n", (const char *)m->B);
		for (pw = UDATA(m)->pwdb; pw; pw = pw->next) {
			if (pw->passwd && strcmp(pw->passwd->pw_name, (const char *)m->B) == 0) {
				UDATA(m)->pwent = pw->passwd;
				goto found;
			}
		}
	} else {
		pn_trace(m, "USER.FIND (by UID) %i\n", m->B);
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

static pn_word uf_user_is_locked(pn_machine *m)
{
	/* FIXME: implement &USER.LOCKED? */
	return 1;
}

static pn_word uf_user_get_uid(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 0;
	return (pn_word)(UDATA(m)->pwent->pw_uid);
}

static pn_word uf_user_get_gid(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 0;
	return (pn_word)(UDATA(m)->pwent->pw_gid);
}

static pn_word uf_user_get_name(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 0;
	return (pn_word)(UDATA(m)->pwent->pw_name);
}

static pn_word uf_user_get_passwd(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 0;
	return (pn_word)(UDATA(m)->pwent->pw_passwd);
}

static pn_word uf_user_get_gecos(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 0;
	return (pn_word)(UDATA(m)->pwent->pw_gecos);
}

static pn_word uf_user_get_home(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 0;
	return (pn_word)(UDATA(m)->pwent->pw_dir);
}

static pn_word uf_user_get_shell(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 0;
	return (pn_word)(UDATA(m)->pwent->pw_shell);
}

static pn_word uf_user_get_pwhash(pn_machine *m)
{
	if (!UDATA(m)->spent) return 0;
	return (pn_word)(UDATA(m)->spent->sp_pwdp);
}

static pn_word uf_user_get_pwmin(pn_machine *m)
{
	if (!UDATA(m)->spent) return 0;
	return (pn_word)(UDATA(m)->spent->sp_min);
}

static pn_word uf_user_get_pwmax(pn_machine *m)
{
	if (!UDATA(m)->spent) return 0;
	return (pn_word)(UDATA(m)->spent->sp_max);
}

static pn_word uf_user_get_pwwarn(pn_machine *m)
{
	if (!UDATA(m)->spent) return 0;
	return (pn_word)(UDATA(m)->spent->sp_warn);
}

static pn_word uf_user_get_inact(pn_machine *m)
{
	if (!UDATA(m)->spent) return 0;
	return (pn_word)(UDATA(m)->spent->sp_inact);
}

static pn_word uf_user_get_expiry(pn_machine *m)
{
	if (!UDATA(m)->spent) return 0;
	return (pn_word)(UDATA(m)->spent->sp_expire);
}

static pn_word uf_user_set_uid(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 1;
	if (UDATA(m)->pwent->pw_uid == (uid_t)m->B) return 0;
	pn_flag(m, CHANGE_FLAG, 1);
	UDATA(m)->pwent->pw_uid = (uid_t)m->B;
	cw_log(LOG_NOTICE, "%s set uid of %s to %i", m->topic,
		UDATA(m)->pwent->pw_name, UDATA(m)->pwent->pw_uid);
	return 0;
}

static pn_word uf_user_set_gid(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 1;
	pn_trace(m, "USER.SET_GID %i\n", m->B);
	if (UDATA(m)->pwent->pw_gid == (gid_t)m->B) return 0;
	pn_flag(m, CHANGE_FLAG, 1);
	UDATA(m)->pwent->pw_gid = (gid_t)m->B;
	cw_log(LOG_NOTICE, "%s set gid of %s to %i", m->topic,
		UDATA(m)->pwent->pw_name, UDATA(m)->pwent->pw_gid);
	return 0;
}

static pn_word uf_user_set_name(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 1;
	if (!UDATA(m)->spent) return 1;

	char *orig = NULL;

	pn_trace(m, "USER.SET_NAME '%s'\n", (const char *)m->B);
	if (cw_strcmp(UDATA(m)->pwent->pw_name, (const char *)m->B) != 0) {
		orig = UDATA(m)->pwent->pw_name;
		UDATA(m)->pwent->pw_name = strdup((const char *)m->B);
		pn_flag(m, CHANGE_FLAG, 1);
	}

	if (cw_strcmp(UDATA(m)->spent->sp_namp, (const char *)m->B) != 0) {
		if (orig)
			free(UDATA(m)->spent->sp_namp);
		else
			orig = UDATA(m)->spent->sp_namp;
		UDATA(m)->spent->sp_namp = strdup((const char *)m->B);
		pn_flag(m, CHANGE_FLAG, 1);
	}

	if (orig) {
		cw_log(LOG_NOTICE, "%s set username of %s to %s",
			m->topic, orig, UDATA(m)->pwent->pw_name);
		free(orig);
	}

	return 0;
}

static pn_word uf_user_set_passwd(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 1;
	pn_trace(m, "USER.SET_PASSWD '%s'\n", (const char *)m->B);
	if (cw_strcmp(UDATA(m)->pwent->pw_passwd, (const char *)m->B) == 0) return 0;
	free(UDATA(m)->pwent->pw_passwd);
	UDATA(m)->pwent->pw_passwd = strdup((const char *)m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	return 0;
}

static pn_word uf_user_set_gecos(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 1;
	pn_trace(m, "USER.SET_GECOS '%s'\n", (const char *)m->B);
	if (cw_strcmp(UDATA(m)->pwent->pw_gecos, (const char *)m->B) == 0) return 0;
	free(UDATA(m)->pwent->pw_gecos);
	UDATA(m)->pwent->pw_gecos = strdup((const char *)m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set comment of %s to '%s'",
		m->topic, UDATA(m)->pwent->pw_name, UDATA(m)->pwent->pw_gecos);
	return 0;
}

static pn_word uf_user_set_home(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 1;
	pn_trace(m, "USER.SET_HOME '%s'\n", (const char *)m->B);
	if (cw_strcmp(UDATA(m)->pwent->pw_dir, (const char *)m->B) == 0) return 0;
	free(UDATA(m)->pwent->pw_dir);
	UDATA(m)->pwent->pw_dir = strdup((const char *)m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set home of %s to '%s'",
		m->topic, UDATA(m)->pwent->pw_name, UDATA(m)->pwent->pw_dir);
	return 0;
}

static pn_word uf_user_set_shell(pn_machine *m)
{
	if (!UDATA(m)->pwent) return 1;
	pn_trace(m, "USER.SET_SHELL '%s'\n", (const char *)m->B);
	if (cw_strcmp(UDATA(m)->pwent->pw_shell, (const char *)m->B) == 0) return 0;
	free(UDATA(m)->pwent->pw_shell);
	UDATA(m)->pwent->pw_shell = strdup((const char *)m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set shell of %s to '%s'",
		m->topic, UDATA(m)->pwent->pw_name, UDATA(m)->pwent->pw_shell);
	return 0;
}

static pn_word uf_user_set_pwhash(pn_machine *m)
{
	if (!UDATA(m)->spent) return 1;
	pn_trace(m, "USER.SET_PWHASH '%s'\n", (const char *)m->B);
	if (cw_strcmp(UDATA(m)->spent->sp_pwdp, (const char *)m->B) == 0) return 0;
	free(UDATA(m)->spent->sp_pwdp);
	UDATA(m)->spent->sp_pwdp = strdup((const char *)m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set password of %s",
		m->topic, UDATA(m)->pwent->pw_name);
	return 0;
}

static pn_word uf_user_set_pwmin(pn_machine *m)
{
	if (!UDATA(m)->spent) return 1;
	pn_trace(m, "USER.SET_PWMIN %li\n", (signed long)m->B);
	if (UDATA(m)->spent->sp_min == (signed long)(m->B)) return 0;
	UDATA(m)->spent->sp_min = (signed long)(m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set minimum password age of %s to %lid",
		m->topic, UDATA(m)->pwent->pw_name, UDATA(m)->spent->sp_min);
	return 0;
}

static pn_word uf_user_set_pwmax(pn_machine *m)
{
	if (!UDATA(m)->spent) return 1;
	pn_trace(m, "USER.SET_PWMAX %li\n", (signed long)m->B);
	if (UDATA(m)->spent->sp_max == (signed long)(m->B)) return 0;
	UDATA(m)->spent->sp_max = (signed long)(m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set maximum password age of %s to %lid",
		m->topic, UDATA(m)->pwent->pw_name, UDATA(m)->spent->sp_max);
	return 0;
}

static pn_word uf_user_set_pwwarn(pn_machine *m)
{
	if (!UDATA(m)->spent) return 1;
	pn_trace(m, "USER.SET_PWWARN %li\n", (signed long)m->B);
	if (UDATA(m)->spent->sp_warn == (signed long)(m->B)) return 0;
	UDATA(m)->spent->sp_warn = (signed long)(m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set password warning period of %s to %lid",
		m->topic, UDATA(m)->pwent->pw_name, UDATA(m)->spent->sp_warn);
	return 0;
}

static pn_word uf_user_set_inact(pn_machine *m)
{
	if (!UDATA(m)->spent) return 1;
	pn_trace(m, "USER.SET_INACT %li\n", (signed long)m->B);
	if (UDATA(m)->spent->sp_inact == (signed long)(m->B)) return 0;
	UDATA(m)->spent->sp_inact = (signed long)(m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set password inactivity period of %s to %lid",
		m->topic, UDATA(m)->pwent->pw_name, UDATA(m)->spent->sp_inact);
	return 0;
}

static pn_word uf_user_set_expiry(pn_machine *m)
{
	if (!UDATA(m)->spent) return 1;
	pn_trace(m, "USER.SET_EXPIRY %li\n", (signed long)m->B);
	if (UDATA(m)->spent->sp_expire == (signed long)(m->B)) return 0;
	UDATA(m)->spent->sp_expire = (signed long)(m->B);
	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s set account expiration of %s to %lid",
		m->topic, UDATA(m)->pwent->pw_name, UDATA(m)->spent->sp_expire);
	return 0;
}

static pn_word uf_user_create(pn_machine *m)
{
	if (!UDATA(m)->pwdb) return 1;

	pn_trace(m, "USER.CREATE '%s' uid=%i gid=%i\n", (const char *)m->A, m->B, m->C);

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
	sp->next->spwd->sp_lstchg = cw_time_s() / 86400;
	sp->next->spwd->sp_min    = -1;
	sp->next->spwd->sp_max    = 99999;
	sp->next->spwd->sp_warn   = 7;
	sp->next->spwd->sp_inact  = -1;
	sp->next->spwd->sp_expire = -1;
	sp->next->spwd->sp_flag   = ~0UL;
	UDATA(m)->spent = sp->next->spwd;

	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s creating user %s (%u:%u) with home %s and shell %s",
		m->topic, UDATA(m)->pwent->pw_name,
		UDATA(m)->pwent->pw_uid, UDATA(m)->pwent->pw_gid,
		UDATA(m)->pwent->pw_dir, UDATA(m)->pwent->pw_shell);
	return 0;
}

static pn_word uf_user_next_uid(pn_machine *m)
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

static pn_word uf_user_remove(pn_machine *m)
{
	if (!UDATA(m)->pwdb || !UDATA(m)->pwent) return 1;
	char *name = NULL;

	struct pwdb *pw, *pwent = NULL;
	for (pw = UDATA(m)->pwdb; pw; pwent = pw, pw = pw->next) {
		if (pw->passwd == UDATA(m)->pwent) {
			pn_flag(m, CHANGE_FLAG, 1);
			name = pw->passwd->pw_name; /* we'll free it later */
			free(pw->passwd->pw_passwd);
			free(pw->passwd->pw_gecos);
			free(pw->passwd->pw_dir);
			free(pw->passwd->pw_shell);
			free(pw->passwd);
			pw->passwd = NULL;

			if (pwent) {
				pwent->next = pw->next;
				free(pw);
			}
			break;
		}
	}

	struct spdb *sp, *spent = NULL;
	for (sp = UDATA(m)->spdb; sp; spent = sp, sp = sp->next) {
		if (sp->spwd == UDATA(m)->spent) {
			pn_flag(m, CHANGE_FLAG, 1);
			if (name)
				free(sp->spwd->sp_namp);
			else
				name = sp->spwd->sp_namp;
			free(sp->spwd->sp_pwdp);
			free(sp->spwd);
			sp->spwd = NULL;

			if (spent) {
				spent->next = sp->next;
				free(sp);
			}
			break;
		}
	}
	if (name) {
		cw_log(LOG_NOTICE, "%s removing user %s", m->topic, name);
		free(name);
	}
	return 0;
}

/*

     ######   ########   #######  ##     ## ########
    ##    ##  ##     ## ##     ## ##     ## ##     ##
    ##        ##     ## ##     ## ##     ## ##     ##
    ##   #### ########  ##     ## ##     ## ########
    ##    ##  ##   ##   ##     ## ##     ## ##
    ##    ##  ##    ##  ##     ## ##     ## ##
     ######   ##     ##  #######   #######  ##

 */

static pn_word uf_group_find(pn_machine *m)
{
	struct grdb *gr;
	struct sgdb *sg;

	if (!UDATA(m)->grdb) pn_die(m, "group database not opened (use GRDB.OPEN)");
	if (!UDATA(m)->sgdb) pn_die(m, "gshadow database not opened (use SGDB.OPEN)");

	if (m->A) {
		pn_trace(m, "GROUP.FIND (by name) '%s'\n", (const char *)m->B);
		for (gr = UDATA(m)->grdb; gr; gr = gr->next) {
			if (gr->group && strcmp(gr->group->gr_name, (const char *)m->B) == 0) {
				UDATA(m)->grent = gr->group;
				goto found;
			}
		}
	} else {
		pn_trace(m, "GROUP.FIND (by GID) %i\n", m->B);
		for (gr = UDATA(m)->grdb; gr; gr = gr->next) {
			if (gr->group && gr->group->gr_gid == (gid_t)m->B) {
				UDATA(m)->grent = gr->group;
				goto found;
			}
		}
	}
	return 1;

found:
	/* now, look for the shadow entry */
	for (sg = UDATA(m)->sgdb; sg; sg = sg->next) {
		if (sg->sgrp && strcmp(sg->sgrp->sg_namp, gr->group->gr_name) == 0) {
			UDATA(m)->sgent = sg->sgrp;
			return 0;
		}
	}
	return 1;
}

static pn_word uf_group_next_gid(pn_machine *m)
{
	if (!UDATA(m)->grdb) return 1;

	struct grdb *db;
	gid_t gid = (gid_t)m->A;
GID: while (gid < 65536) {
		for (db = UDATA(m)->grdb; db; db = db->next) {
			if (db->group->gr_gid != gid) continue;
			gid++; goto GID;
		}
		m->B = (pn_word)gid;
		return 0;
	}
	return 1;
}

static pn_word uf_group_create(pn_machine *m)
{
	if (!UDATA(m)->grdb) return 1;

	pn_trace(m, "GROUP.CREATE '%s' gid=%i\n", (const char *)m->A, m->B);

	struct grdb *gr;
	for (gr = UDATA(m)->grdb; gr->next; gr = gr->next)
		;

	gr->next = calloc(1, sizeof(struct grdb));
	gr->next->group = calloc(1, sizeof(struct group));
	gr->next->group->gr_name   = strdup((const char *)m->A);
	gr->next->group->gr_gid    = (uid_t)m->B;
	gr->next->group->gr_passwd = strdup("x");
	UDATA(m)->grent = gr->next->group;

	struct sgdb *sg;
	for (sg = UDATA(m)->sgdb; sg->next; sg = sg->next)
		;

	sg->next = calloc(1, sizeof(struct sgdb));
	sg->next->sgrp = calloc(1, sizeof(struct sgrp));
	sg->next->sgrp->sg_namp   = strdup((const char *)m->A);
	sg->next->sgrp->sg_passwd = strdup("*");
	sg->next->sgrp->sg_mem    = calloc(1, sizeof(char *));
	sg->next->sgrp->sg_adm    = calloc(1, sizeof(char *));
	UDATA(m)->sgent = sg->next->sgrp;

	pn_flag(m, CHANGE_FLAG, 1);
	cw_log(LOG_NOTICE, "%s creating group %s (%u)", m->topic,
		UDATA(m)->grent->gr_name, UDATA(m)->grent->gr_gid);
	return 0;
}

static pn_word uf_group_remove(pn_machine *m)
{
	if (!UDATA(m)->grdb || !UDATA(m)->grent) return 1;
	if (!UDATA(m)->sgdb || !UDATA(m)->sgent) return 1;

	char *name = NULL;
	struct grdb *gr, *grent = NULL;
	for (gr = UDATA(m)->grdb; gr; grent = gr, gr = gr->next) {
		if (gr->group == UDATA(m)->grent) {
			pn_flag(m, CHANGE_FLAG, 1);
			name = gr->group->gr_name;
			free(gr->group->gr_passwd);
			s_strlist_free(gr->group->gr_mem);
			free(gr->group);
			gr->group = NULL;

			if (grent) {
				grent->next = gr->next;
				free(gr);
			}
			break;
		}
	}

	struct sgdb *sg, *sgent = NULL;
	for (sg = UDATA(m)->sgdb; sg; sgent = sg, sg = sg->next) {
		if (sg->sgrp == UDATA(m)->sgent) {
			pn_flag(m, CHANGE_FLAG, 1);
			if (name)
				free(sg->sgrp->sg_namp);
			else
				name = sg->sgrp->sg_namp;
			free(sg->sgrp->sg_passwd);
			s_strlist_free(sg->sgrp->sg_mem);
			s_strlist_free(sg->sgrp->sg_adm);
			free(sg->sgrp);
			sg->sgrp = NULL;

			if (sgent) {
				sgent->next = sg->next;
				free(sg);
			}
			break;
		}
	}
	if (name) {
		cw_log(LOG_NOTICE, "%s removing group %s", m->topic, name);
		free(name);
	}
	return 0;
}

static pn_word uf_group_has_member(pn_machine *m)
{
	if (!UDATA(m)->grent) return 1;
	pn_trace(m, "GROUP.HAS_MEMBER? '%s'\n", (const char *)m->A);

	char **l;
	for (l = UDATA(m)->grent->gr_mem; *l; l++) {
		if (strcmp(*l, (const char *)m->A) == 0) return 0;
	}
	return 1;
}

static pn_word uf_group_has_admin(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	pn_trace(m, "GROUP.HAS_ADMIN? '%s'\n", (const char *)m->A);

	char **l;
	for (l = UDATA(m)->sgent->sg_adm; *l; l++) {
		if (strcmp(*l, (const char *)m->A) == 0) return 0;
	}
	return 1;
}

static pn_word uf_group_rm_member(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	pn_trace(m, "GROUP.RM_MEMBER '%s'\n", (const char *)m->A);
	UDATA(m)->grent->gr_mem = s_strlist_remove(
		UDATA(m)->grent->gr_mem, (const char *)m->A);
	UDATA(m)->sgent->sg_mem = s_strlist_remove(
		UDATA(m)->sgent->sg_mem, (const char *)m->A);
	cw_log(LOG_NOTICE, "%s removing user %s from the member list",
		m->topic, (const char *)m->A);
	return 0;
}

static pn_word uf_group_rm_admin(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	pn_trace(m, "GROUP.RM_ADMIN '%s'\n", (const char *)m->A);
	UDATA(m)->sgent->sg_adm = s_strlist_remove(
		UDATA(m)->sgent->sg_adm, (const char *)m->A);
	cw_log(LOG_NOTICE, "%s removing user %s from the admin list",
		m->topic, (const char *)m->A);
	return 0;
}

static pn_word uf_group_add_member(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	pn_trace(m, "GROUP.ADD_MEMBER '%s'\n", (const char *)m->A);
	UDATA(m)->grent->gr_mem = s_strlist_add(
		UDATA(m)->grent->gr_mem, (const char *)m->A);
	UDATA(m)->sgent->sg_mem = s_strlist_add(
		UDATA(m)->sgent->sg_mem, (const char *)m->A);
	cw_log(LOG_NOTICE, "%s adding user %s to the member list",
		m->topic, (const char *)m->A);
	return 0;
}

static pn_word uf_group_add_admin(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	pn_trace(m, "GROUP.ADD_ADMIN '%s'\n", (const char *)m->A);
	UDATA(m)->sgent->sg_adm = s_strlist_add(
		UDATA(m)->sgent->sg_adm, (const char *)m->A);
	cw_log(LOG_NOTICE, "%s adding user %s to the admin list",
		m->topic, (const char *)m->A);
	return 0;
}

static pn_word uf_group_get_gid(pn_machine *m)
{
	if (!UDATA(m)->grent) return 1;
	return (pn_word)(UDATA(m)->grent->gr_gid);
}

static pn_word uf_group_get_name(pn_machine *m)
{
	if (!UDATA(m)->grent) return 1;
	return (pn_word)(UDATA(m)->grent->gr_name);
}

static pn_word uf_group_get_passwd(pn_machine *m)
{
	if (!UDATA(m)->grent) return 1;
	return (pn_word)(UDATA(m)->grent->gr_passwd);
}

static pn_word uf_group_get_pwhash(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	return (pn_word)(UDATA(m)->sgent->sg_passwd);
}

static pn_word uf_group_set_gid(pn_machine *m)
{
	if (!UDATA(m)->grent) return 1;
	if (UDATA(m)->grent->gr_gid == (gid_t)m->B) return 0;
	pn_flag(m, CHANGE_FLAG, 1);
	UDATA(m)->grent->gr_gid = (gid_t)m->B;
	cw_log(LOG_NOTICE, "%s changing gid of %s to %u",
		m->topic, UDATA(m)->grent->gr_name, UDATA(m)->grent->gr_gid);
	return 0;
}

static pn_word uf_group_set_name(pn_machine *m)
{
	if (!UDATA(m)->grent) return 1;
	if (!UDATA(m)->sgent) return 1;

	char *orig = NULL;
	pn_trace(m, "GROUP.SET_NAME '%s'\n", (const char *)m->B);

	if (cw_strcmp(UDATA(m)->grent->gr_name, (const char *)m->B) != 0) {
		orig = UDATA(m)->grent->gr_name;
		UDATA(m)->grent->gr_name = strdup((const char *)m->B);
		pn_flag(m, CHANGE_FLAG, 1);
	}

	if (cw_strcmp(UDATA(m)->sgent->sg_namp, (const char *)m->B) != 0) {
		if (orig)
			free(UDATA(m)->sgent->sg_namp);
		else
			orig = UDATA(m)->sgent->sg_namp;
		UDATA(m)->sgent->sg_namp = strdup((const char *)m->B);
		pn_flag(m, CHANGE_FLAG, 1);
	}

	if (orig) {
		cw_log(LOG_NOTICE, "%s changing name of %s to %s",
			m->topic, orig, UDATA(m)->grent->gr_name);
		free(orig);
	}
	return 0;
}

static pn_word uf_group_set_passwd(pn_machine *m)
{
	if (!UDATA(m)->grent) return 1;
	pn_trace(m, "GROUP.SET_PASSWD '%s'\n", (const char *)m->B);
	if (cw_strcmp(UDATA(m)->grent->gr_passwd, (const char *)m->B) == 0) return 0;
	free(UDATA(m)->grent->gr_passwd);
	UDATA(m)->grent->gr_passwd = strdup((const char *)m->B);
	return 0;
}

static pn_word uf_group_set_pwhash(pn_machine *m)
{
	if (!UDATA(m)->grent) return 1;
	pn_trace(m, "GROUP.SET_PWHASH '%s'\n", (const char *)m->B);
	if (cw_strcmp(UDATA(m)->sgent->sg_passwd, (const char *)m->B) == 0) return 0;
	free(UDATA(m)->sgent->sg_passwd);
	UDATA(m)->sgent->sg_passwd = strdup((const char *)m->B);
	cw_log(LOG_NOTICE, "%s set password of group %s", m->topic, UDATA(m)->grent->gr_name);
	return 0;
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


int pendulum_init(pn_machine *m, void *zconn)
{
	pn_func(m,  "FS.EXISTS?",         uf_fs_exists);
	pn_func(m,  "FS.IS_FILE?",        uf_fs_is_file);
	pn_func(m,  "FS.IS_DIR?",         uf_fs_is_dir);
	pn_func(m,  "FS.IS_CHARDEV?",     uf_fs_is_chardev);
	pn_func(m,  "FS.IS_BLOCKDEV?",    uf_fs_is_blockdev);
	pn_func(m,  "FS.IS_FIFO?",        uf_fs_is_fifo);
	pn_func(m,  "FS.IS_SYMLINK?",     uf_fs_is_symlink);
	pn_func(m,  "FS.IS_SOCKET?",      uf_fs_is_socket);
	pn_func(m,  "FS.SHA1",            uf_fs_sha1);

	pn_func(m,  "FS.MKDIR",           uf_fs_mkdir);
	pn_func(m,  "FS.MKFILE",          uf_fs_mkfile);
	pn_func(m,  "FS.PUT",             uf_fs_put);
	pn_func(m,  "FS.GET",             uf_fs_get);
	pn_func(m,  "FS.SYMLINK",         uf_fs_symlink);
	pn_func(m,  "FS.HARDLINK",        uf_fs_link);
	pn_func(m,  "FS.COPY_R",          uf_fs_copy_r);

	pn_func(m,  "FS.UNLINK",          uf_fs_unlink);
	pn_func(m,  "FS.RMDIR",           uf_fs_rmdir);

	pn_func(m,  "FS.CHOWN",           uf_fs_chown);
	pn_func(m,  "FS.CHMOD",           uf_fs_chmod);

	pn_func(m,  "USERDB.OPEN",        uf_userdb_open);
	pn_func(m,  "USERDB.SAVE",        uf_userdb_save);
	pn_func(m,  "USERDB.CLOSE",       uf_userdb_close);

	pn_func(m,  "USER.FIND",          uf_user_find);
	pn_func(m,  "USER.NEXT_UID",      uf_user_next_uid);
	pn_func(m,  "USER.CREATE",        uf_user_create);
	pn_func(m,  "USER.REMOVE",        uf_user_remove);

	pn_func(m,  "USER.LOCKED?",       uf_user_is_locked);

	pn_func(m,  "USER.GET_UID",       uf_user_get_uid);
	pn_func(m,  "USER.GET_GID",       uf_user_get_gid);
	pn_func(m,  "USER.GET_NAME",      uf_user_get_name);
	pn_func(m,  "USER.GET_PASSWD",    uf_user_get_passwd);
	pn_func(m,  "USER.GET_GECOS",     uf_user_get_gecos);
	pn_func(m,  "USER.GET_HOME",      uf_user_get_home);
	pn_func(m,  "USER.GET_SHELL",     uf_user_get_shell);
	pn_func(m,  "USER.GET_PWHASH",    uf_user_get_pwhash);
	pn_func(m,  "USER.GET_PWMIN",     uf_user_get_pwmin);
	pn_func(m,  "USER.GET_PWMAX",     uf_user_get_pwmax);
	pn_func(m,  "USER.GET_PWWARN",    uf_user_get_pwwarn);
	pn_func(m,  "USER.GET_INACT",     uf_user_get_inact);
	pn_func(m,  "USER.GET_EXPIRY",    uf_user_get_expiry);

	pn_func(m,  "USER.SET_UID",       uf_user_set_uid);
	pn_func(m,  "USER.SET_GID",       uf_user_set_gid);
	pn_func(m,  "USER.SET_NAME",      uf_user_set_name);
	pn_func(m,  "USER.SET_PASSWD",    uf_user_set_passwd);
	pn_func(m,  "USER.SET_GECOS",     uf_user_set_gecos);
	pn_func(m,  "USER.SET_HOME",      uf_user_set_home);
	pn_func(m,  "USER.SET_SHELL",     uf_user_set_shell);
	pn_func(m,  "USER.SET_PWHASH",    uf_user_set_pwhash);
	pn_func(m,  "USER.SET_PWMIN",     uf_user_set_pwmin);
	pn_func(m,  "USER.SET_PWMAX",     uf_user_set_pwmax);
	pn_func(m,  "USER.SET_PWWARN",    uf_user_set_pwwarn);
	pn_func(m,  "USER.SET_INACT",     uf_user_set_inact);
	pn_func(m,  "USER.SET_EXPIRY",    uf_user_set_expiry);

	pn_func(m,  "GROUP.FIND",         uf_group_find);
	pn_func(m,  "GROUP.NEXT_GID",     uf_group_next_gid);
	pn_func(m,  "GROUP.CREATE",       uf_group_create);
	pn_func(m,  "GROUP.REMOVE",       uf_group_remove);

	pn_func(m,  "GROUP.HAS_ADMIN?",   uf_group_has_admin);
	pn_func(m,  "GROUP.HAS_MEMBER?",  uf_group_has_member);
	pn_func(m,  "GROUP.RM_ADMIN",     uf_group_rm_admin);
	pn_func(m,  "GROUP.RM_MEMBER",    uf_group_rm_member);
	pn_func(m,  "GROUP.ADD_ADMIN",    uf_group_add_admin);
	pn_func(m,  "GROUP.ADD_MEMBER",   uf_group_add_member);

	pn_func(m,  "GROUP.GET_GID",      uf_group_get_gid);
	pn_func(m,  "GROUP.GET_NAME",     uf_group_get_name);
	pn_func(m,  "GROUP.GET_PASSWD",   uf_group_get_passwd);
	pn_func(m,  "GROUP.GET_PWHASH",   uf_group_get_pwhash);

	pn_func(m,  "GROUP.SET_GID",      uf_group_set_gid);
	pn_func(m,  "GROUP.SET_NAME",     uf_group_set_name);
	pn_func(m,  "GROUP.SET_PASSWD",   uf_group_set_passwd);
	pn_func(m,  "GROUP.SET_PWHASH",   uf_group_set_pwhash);

	pn_func(m,  "AUGEAS.INIT",        uf_aug_init);
	pn_func(m,  "AUGEAS.SYSERR",      uf_aug_syserr);
	pn_func(m,  "AUGEAS.SAVE",        uf_aug_save);
	pn_func(m,  "AUGEAS.CLOSE",       uf_aug_close);
	pn_func(m,  "AUGEAS.SET",         uf_aug_set);
	pn_func(m,  "AUGEAS.GET",         uf_aug_get);
	pn_func(m,  "AUGEAS.FIND",        uf_aug_find);
	pn_func(m,  "AUGEAS.REMOVE",      uf_aug_remove);

	pn_func(m,  "UTIL.VERCMP",        uf_util_vercmp);

	pn_func(m,  "EXEC.CHECK",         uf_exec_check);
	pn_func(m,  "EXEC.RUN1",          uf_exec_run1);

	pn_func(m,  "SERVER.SHA1",        uf_server_sha1);
	pn_func(m,  "SERVER.WRITEFILE",   uf_server_writefile);

	m->U = calloc(1, sizeof(udata));
	m->pragma = uf_pragma;
	UDATA(m)->zconn = zconn;
	uf_pragma(m, "userdb.root", "");

	return 0;
}

int pendulum_destroy(pn_machine *m)
{
	_sgdb_close(m); _grdb_close(m);
	_spdb_close(m); _pwdb_close(m);

	free(UDATA(m)->aug_last);
	aug_close(UDATA(m)->aug_ctx);

	free(m->U);
	return 0;
}
