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

#include "pendulum_funcs.h"
#include "cw.h"
#include "sha1.h"

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
	if (!a || !*a) return;
	s = a;
	while (*s) {
		free(*s++);
	}
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

#define EXEC_OUTPUT_MAX 256
static int s_exec(const char *cmd, char **out, char **err, uid_t uid, gid_t gid)
{
	int proc_stat;
	int outfd[2], errfd[2];
	size_t n;
	pid_t pid;
	fd_set fds;
	int nfds = 0;
	int read_stdout = (out ? 1 : 0);
	int read_stderr = (err ? 1 : 0);
	int nullfd;

	nullfd = open("/dev/null", O_WRONLY);
	if (read_stdout && pipe(outfd) != 0) { return 254; }
	if (read_stderr && pipe(errfd) != 0) { return 254; }

	switch (pid = fork()) {

	case -1: /* failed to fork */
		if (read_stdout) {
			close(outfd[0]);
			close(outfd[1]);
		}
		if (read_stderr) {
			close(errfd[0]);
			close(errfd[1]);
		}
		return 255;

	case 0: /* in child */
		close(0);
		dup2((read_stdout ? outfd[1] : nullfd), 1);
		dup2((read_stderr ? errfd[1] : nullfd), 2);

		if (gid && setgid(gid) != 0)
			cw_log(LOG_WARNING, "EXEC.CHECK child could not switch to group ID %u to run `%s'", gid, cmd);

		if (uid && setuid(uid) != 0)
			cw_log(LOG_WARNING, "EXEC.CHECK child could not switch to user ID %u to run `%s'", uid, cmd);

		execl("/bin/sh", "sh", "-c", cmd, (char*)NULL);
		exit(1);

	default: /* in parent */
		if (!read_stdout && !read_stderr) { break; }

		FD_ZERO(&fds);

		if (read_stderr) {
			close(errfd[1]);
			FD_SET(errfd[0], &fds);
			nfds = (nfds > errfd[0] ? nfds : errfd[0]);
		}
		if (read_stdout) {
			close(outfd[1]);
			FD_SET(outfd[0], &fds);
			nfds = (nfds > outfd[0] ? nfds : outfd[0]);
		}

		nfds++;
		while ((read_stdout || read_stderr) && select(nfds, &fds, NULL, NULL, NULL) > 0) {
			if (read_stdout && FD_ISSET(outfd[0], &fds)) {
				*out = calloc(EXEC_OUTPUT_MAX, sizeof(char));
				n = read(outfd[0], *out, EXEC_OUTPUT_MAX);
				(*out)[n] = '\0';

				FD_CLR(outfd[0], &fds);
				close(outfd[0]);
				read_stdout = 0;
			}

			if (read_stderr && FD_ISSET(errfd[0], &fds)) {
				*err = calloc(EXEC_OUTPUT_MAX, sizeof(char));
				n = read(errfd[0], *err, EXEC_OUTPUT_MAX);
				(*err)[n] = '\0';

				FD_CLR(errfd[0], &fds);
				close(errfd[0]);
				read_stderr = 0;
			}

			if (read_stdout) { FD_SET(outfd[0], &fds); }
			if (read_stderr) { FD_SET(errfd[0], &fds); }
		}
	}

	waitpid(pid, &proc_stat, 0);
	if (!WIFEXITED(proc_stat)) return 255;
	return WEXITSTATUS(proc_stat);
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

	char          *exec_last;

	augeas        *aug_ctx;
	const char    *aug_last;

	char          *rsha1;
	struct SHA1    lsha1;
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

static pn_word cwa_pragma(pn_machine *m, const char *k, const char *v)
{
	if (strcmp(k, "userdb.root") == 0) {
		size_t n;
		n = snprintf(UDATA(m)->etc_passwd,  1023, "%s%s", v, "/etc/passwd");
		UDATA(m)->etc_passwd[n] = '\0';

		n = snprintf(UDATA(m)->etc_shadow,  1023, "%s%s", v, "/etc/shadow");
		UDATA(m)->etc_shadow[n] = '\0';

		n = snprintf(UDATA(m)->etc_group,   1023, "%s%s", v, "/etc/group");
		UDATA(m)->etc_group[n] = '\0';

		n = snprintf(UDATA(m)->etc_gshadow, 1023, "%s%s", v, "/etc/gshadow");
		UDATA(m)->etc_gshadow[n] = '\0';
		return 0;
	}

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

static pn_word cwa_fs_sha1(pn_machine *m)
{
	int rc = sha1_file((const char *)m->A, &(UDATA(m)->lsha1));
	m->S2 = (pn_word)(UDATA(m)->lsha1.hex);
	return rc;
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

static pn_word cwa_fs_put(pn_machine *m)
{
	FILE *output = fopen((const char *)m->A, "w");
	if (!output) return 1;

	size_t n = fprintf(output, "%s", (const char *)m->B);
	fclose(output);
	return n == strlen((const char *)m->B) ? 0 : 1;
}

static pn_word cwa_fs_get(pn_machine *m)
{
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

static pn_word cwa_aug_init(pn_machine *m)
{
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

static pn_word cwa_aug_syserr(pn_machine *m)
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

static pn_word cwa_aug_save(pn_machine *m)
{
	return aug_save(UDATA(m)->aug_ctx);
}

static pn_word cwa_aug_close(pn_machine *m)
{
	aug_close(UDATA(m)->aug_ctx);
	UDATA(m)->aug_ctx = NULL;
	return 0;
}

static pn_word cwa_aug_set(pn_machine *m)
{
	char *path = s_format((const char *)m->A, m->C, m->D, m->E, m->F);
	int rc = aug_set(UDATA(m)->aug_ctx, path, (const char *)m->B);
	free(path);
	return rc;
}

static pn_word cwa_aug_get(pn_machine *m)
{
	char *path = s_format((const char *)m->A, m->C, m->D, m->E, m->F);
	int rc = aug_get(UDATA(m)->aug_ctx, path, &(UDATA(m)->aug_last));
	m->S2 = (pn_word)(UDATA(m)->aug_last);
	free(path);
	return rc == 1 ? 0 : 1;
}

static pn_word cwa_aug_find(pn_machine *m)
{
	char **r = NULL;
	char *path = s_format((const char *)m->A, m->C, m->D, m->E, m->F);
	int rc = aug_match(UDATA(m)->aug_ctx, path, &r);
	free(path);
	if (rc != 1) return 1;

	UDATA(m)->aug_last = strdup(r[0]);
	m->S2 = (pn_word)(UDATA(m)->aug_last);
	free(r);
	return 0;
}

static pn_word cwa_aug_remove(pn_machine *m)
{
	char *path = s_format((const char *)m->A, m->C, m->D, m->E, m->F);
	int rc = aug_rm(UDATA(m)->aug_ctx, (const char *)m->A) > 1 ? 0 : 1;
	free(path);
	return rc;
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

static pn_word cwa_exec_check(pn_machine *m)
{
	return s_exec((const char *)m->A, NULL, NULL,
		(uid_t)m->B, (gid_t)m->C);
}

static pn_word cwa_exec_run1(pn_machine *m)
{
	char *out, *p;
	int rc = s_exec((const char *)m->A, &out, NULL,
		(uid_t)m->B, (gid_t)m->C);

	for (p = out; *p && *p != '\n'; p++);
	*p = '\0';

	free(UDATA(m)->exec_last);
	m->S2 = (pn_word)(UDATA(m)->exec_last = out);
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

static pn_word cwa_server_sha1(pn_machine *m)
{
	cw_pdu_t *pdu, *reply;
	pdu = cw_pdu_make(NULL, 2, "FILE", (const char *)m->A);
	int rc = cw_pdu_send(UDATA(m)->zconn, pdu);
	assert(rc == 0);

	reply = cw_pdu_recv(UDATA(m)->zconn);
	if (!reply) {
		cw_log(LOG_ERR, "failed: %s", zmq_strerror(errno));
		return 1;
	}
	cw_log(LOG_INFO, "Received a '%s' PDU", reply->type);
	if (strcmp(reply->type, "ERROR") == 0) {
		char *e = cw_pdu_text(reply, 1);
		cw_log(LOG_ERR, "failed: %s", e);
		free(e);
		return 1;
	}

	if (strcmp(reply->type, "SHA1") == 0) {
		free(UDATA(m)->rsha1);
		m->S2 = (pn_word)(UDATA(m)->rsha1 = cw_pdu_text(reply, 1));
		return 0;
	}

	cw_log(LOG_ERR, "Unexpected PDU received.");
	return 1;
}

static pn_word cwa_server_writefile(pn_machine *m)
{
	FILE *io = NULL;
	int rc, n = 0;
	char size[16];
	cw_pdu_t *pdu, *reply;

	for (;;) {
		rc = snprintf(size, 15, "%i", n++);
		assert(rc > 0);
		pdu = cw_pdu_make(NULL, 2, "DATA", size);
		rc = cw_pdu_send(UDATA(m)->zconn, pdu);
		assert(rc == 0);

		reply = cw_pdu_recv(UDATA(m)->zconn);
		if (!reply) {
			cw_log(LOG_ERR, "failed: %s", zmq_strerror(errno));
			if (io) fclose(io);
			return 1;
		}
		cw_log(LOG_INFO, "server.writefile: received a %s PDU", reply->type);

		if (strcmp(reply->type, "EOF") == 0) {
			if (io) fclose(io);
			return 0;
		}

		if (!io) io = fopen((const char *)m->A, "w");
		if (!io) {
			cw_log(LOG_ERR, "failed: %s", strerror(errno));
			return 1;
		}

		char *data = cw_pdu_text(reply, 1);
		fprintf(io, "%s", data);
		free(data);
	}
	if (io) fclose(io);

	return 0;
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
	return 0;
}

static pn_word cwa_userdb_open(pn_machine *m)
{
	if (_pwdb_open(m) == 0 && _spdb_open(m) == 0
	 && _grdb_open(m) == 0 && _sgdb_open(m) == 0) return 0;

	_sgdb_close(m); _grdb_close(m);
	_spdb_close(m); _pwdb_close(m);
	return 1;
}

static pn_word cwa_userdb_close(pn_machine *m)
{
	_sgdb_close(m); _grdb_close(m);
	_spdb_close(m); _pwdb_close(m);
	return 0;
}

static pn_word cwa_userdb_save(pn_machine *m)
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

static pn_word cwa_user_find(pn_machine *m)
{
	struct pwdb *pw;
	struct spdb *sp;

	if (!UDATA(m)->pwdb) pn_die(m, "passwd database not opened (use USERDB.OPEN)");
	if (!UDATA(m)->spdb) pn_die(m, "shadow database not opened (use USERDB.OPEN)");

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
	UDATA(m)->spent->sp_min = (signed long)(m->B);
	return 0;
}

static pn_word cwa_user_set_pwmax(pn_machine *m)
{
	UDATA(m)->spent->sp_max = (signed long)(m->B);
	return 0;
}

static pn_word cwa_user_set_pwwarn(pn_machine *m)
{
	UDATA(m)->spent->sp_warn = (signed long)(m->B);
	return 0;
}

static pn_word cwa_user_set_inact(pn_machine *m)
{
	UDATA(m)->spent->sp_inact = (signed long)(m->B);
	return 0;
}

static pn_word cwa_user_set_expiry(pn_machine *m)
{
	UDATA(m)->spent->sp_expire = (signed long)(m->B);
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

	struct pwdb *pw, *pwent = NULL;
	for (pw = UDATA(m)->pwdb; pw; pwent = pw, pw = pw->next) {
		if (pw->passwd == UDATA(m)->pwent) {
			free(pw->passwd->pw_name);
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
			free(sp->spwd->sp_namp);
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

static pn_word cwa_group_find(pn_machine *m)
{
	struct grdb *gr;
	struct sgdb *sg;

	if (!UDATA(m)->grdb) pn_die(m, "group database not opened (use GRDB.OPEN)");
	if (!UDATA(m)->sgdb) pn_die(m, "gshadow database not opened (use SGDB.OPEN)");

	if (m->A) {
		for (gr = UDATA(m)->grdb; gr; gr = gr->next) {
			if (gr->group && strcmp(gr->group->gr_name, (const char *)m->B) == 0) {
				UDATA(m)->grent = gr->group;
				goto found;
			}
		}
	} else {
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

static pn_word cwa_group_next_gid(pn_machine *m)
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

static pn_word cwa_group_create(pn_machine *m)
{
	if (!UDATA(m)->grdb) return 1;

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

	return 0;
}

static pn_word cwa_group_remove(pn_machine *m)
{
	if (!UDATA(m)->grdb || !UDATA(m)->grent) return 1;
	if (!UDATA(m)->sgdb || !UDATA(m)->sgent) return 1;

	struct grdb *gr, *grent = NULL;
	for (gr = UDATA(m)->grdb; gr; grent = gr, gr = gr->next) {
		if (gr->group == UDATA(m)->grent) {
			free(gr->group->gr_name);
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
			free(sg->sgrp->sg_namp);
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
	return 0;
}

static pn_word cwa_group_has_member(pn_machine *m)
{
	if (!UDATA(m)->grent) return 1;

	char **l;
	for (l = UDATA(m)->grent->gr_mem; *l; l++) {
		if (strcmp(*l, (const char *)m->A) == 0) return 0;
	}
	return 1;
}

static pn_word cwa_group_has_admin(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;

	char **l;
	for (l = UDATA(m)->sgent->sg_adm; *l; l++) {
		if (strcmp(*l, (const char *)m->A) == 0) return 0;
	}
	return 1;
}

static pn_word cwa_group_rm_member(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	UDATA(m)->grent->gr_mem = s_strlist_remove(
		UDATA(m)->grent->gr_mem, (const char *)m->A);
	UDATA(m)->sgent->sg_mem = s_strlist_remove(
		UDATA(m)->sgent->sg_mem, (const char *)m->A);
	return 0;
}

static pn_word cwa_group_rm_admin(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	UDATA(m)->sgent->sg_adm = s_strlist_remove(
		UDATA(m)->sgent->sg_adm, (const char *)m->A);
	return 0;
}

static pn_word cwa_group_add_member(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	UDATA(m)->grent->gr_mem = s_strlist_add(
		UDATA(m)->grent->gr_mem, (const char *)m->A);
	UDATA(m)->sgent->sg_mem = s_strlist_add(
		UDATA(m)->sgent->sg_mem, (const char *)m->A);
	return 0;
}

static pn_word cwa_group_add_admin(pn_machine *m)
{
	if (!UDATA(m)->sgent) return 1;
	UDATA(m)->sgent->sg_adm = s_strlist_add(
		UDATA(m)->sgent->sg_adm, (const char *)m->A);
	return 0;
}

static pn_word cwa_group_get_gid(pn_machine *m)
{
	return (pn_word)(UDATA(m)->grent->gr_gid);
}

static pn_word cwa_group_get_name(pn_machine *m)
{
	return (pn_word)(UDATA(m)->grent->gr_name);
}

static pn_word cwa_group_get_passwd(pn_machine *m)
{
	return (pn_word)(UDATA(m)->grent->gr_passwd);
}

static pn_word cwa_group_get_pwhash(pn_machine *m)
{
	return (pn_word)(UDATA(m)->sgent->sg_passwd);
}

static pn_word cwa_group_set_gid(pn_machine *m)
{
	UDATA(m)->grent->gr_gid = (gid_t)m->B;
	return 0;
}

static pn_word cwa_group_set_name(pn_machine *m)
{
	free(UDATA(m)->grent->gr_name);
	UDATA(m)->grent->gr_name = strdup((const char *)m->B);

	free(UDATA(m)->sgent->sg_namp);
	UDATA(m)->sgent->sg_namp = strdup((const char *)m->B);

	return 0;
}

static pn_word cwa_group_set_passwd(pn_machine *m)
{
	free(UDATA(m)->grent->gr_passwd);
	UDATA(m)->grent->gr_passwd = strdup((const char *)m->B);
	return 0;
}

static pn_word cwa_group_set_pwhash(pn_machine *m)
{
	free(UDATA(m)->sgent->sg_passwd);
	UDATA(m)->sgent->sg_passwd = strdup((const char *)m->B);
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


int pendulum_funcs(pn_machine *m, void *zconn)
{
	pn_func(m,  "FS.EXISTS?",         cwa_fs_exists);
	pn_func(m,  "FS.FILE?",           cwa_fs_is_file);
	pn_func(m,  "FS.DIR?",            cwa_fs_is_dir);
	pn_func(m,  "FS.CHARDEV?",        cwa_fs_is_chardev);
	pn_func(m,  "FS.BLOCKDEV?",       cwa_fs_is_blockdev);
	pn_func(m,  "FS.FIFO?",           cwa_fs_is_fifo);
	pn_func(m,  "FS.SYMLINK?",        cwa_fs_is_symlink);
	pn_func(m,  "FS.SOCKET?",         cwa_fs_is_socket);
	pn_func(m,  "FS.SHA1",            cwa_fs_sha1);

	pn_func(m,  "FS.MKDIR",           cwa_fs_mkdir);
	pn_func(m,  "FS.MKFILE",          cwa_fs_mkfile);
	pn_func(m,  "FS.PUT",             cwa_fs_put);
	pn_func(m,  "FS.GET",             cwa_fs_get);
	pn_func(m,  "FS.SYMLINK",         cwa_fs_symlink);
	pn_func(m,  "FS.HARDLINK",        cwa_fs_link);

	pn_func(m,  "FS.UNLINK",          cwa_fs_unlink);
	pn_func(m,  "FS.RMDIR",           cwa_fs_rmdir);

	pn_func(m,  "FS.CHOWN",           cwa_fs_chown);
	pn_func(m,  "FS.CHMOD",           cwa_fs_chmod);

	pn_func(m,  "USERDB.OPEN",        cwa_userdb_open);
	pn_func(m,  "USERDB.SAVE",        cwa_userdb_save);
	pn_func(m,  "USERDB.CLOSE",       cwa_userdb_close);

	pn_func(m,  "USER.FIND",          cwa_user_find);
	pn_func(m,  "USER.NEXT_UID",      cwa_user_next_uid);
	pn_func(m,  "USER.CREATE",        cwa_user_create);
	pn_func(m,  "USER.REMOVE",        cwa_user_remove);

	pn_func(m,  "USER.LOCKED?",       cwa_user_is_locked);

	pn_func(m,  "USER.GET_UID",       cwa_user_get_uid);
	pn_func(m,  "USER.GET_GID",       cwa_user_get_gid);
	pn_func(m,  "USER.GET_NAME",      cwa_user_get_name);
	pn_func(m,  "USER.GET_PASSWD",    cwa_user_get_passwd);
	pn_func(m,  "USER.GET_GECOS",     cwa_user_get_gecos);
	pn_func(m,  "USER.GET_HOME",      cwa_user_get_home);
	pn_func(m,  "USER.GET_SHELL",     cwa_user_get_shell);
	pn_func(m,  "USER.GET_PWHASH",    cwa_user_get_pwhash);
	pn_func(m,  "USER.GET_PWMIN",     cwa_user_get_pwmin);
	pn_func(m,  "USER.GET_PWMAX",     cwa_user_get_pwmax);
	pn_func(m,  "USER.GET_PWWARN",    cwa_user_get_pwwarn);
	pn_func(m,  "USER.GET_INACT",     cwa_user_get_inact);
	pn_func(m,  "USER.GET_EXPIRY",    cwa_user_get_expiry);

	pn_func(m,  "USER.SET_UID",       cwa_user_set_uid);
	pn_func(m,  "USER.SET_GID",       cwa_user_set_gid);
	pn_func(m,  "USER.SET_NAME",      cwa_user_set_name);
	pn_func(m,  "USER.SET_PASSWD",    cwa_user_set_passwd);
	pn_func(m,  "USER.SET_GECOS",     cwa_user_set_gecos);
	pn_func(m,  "USER.SET_HOME",      cwa_user_set_home);
	pn_func(m,  "USER.SET_SHELL",     cwa_user_set_shell);
	pn_func(m,  "USER.SET_PWHASH",    cwa_user_set_pwhash);
	pn_func(m,  "USER.SET_PWMIN",     cwa_user_set_pwmin);
	pn_func(m,  "USER.SET_PWMAX",     cwa_user_set_pwmax);
	pn_func(m,  "USER.SET_PWWARN",    cwa_user_set_pwwarn);
	pn_func(m,  "USER.SET_INACT",     cwa_user_set_inact);
	pn_func(m,  "USER.SET_EXPIRY",    cwa_user_set_expiry);

	pn_func(m,  "GROUP.FIND",         cwa_group_find);
	pn_func(m,  "GROUP.NEXT_GID",     cwa_group_next_gid);
	pn_func(m,  "GROUP.CREATE",       cwa_group_create);
	pn_func(m,  "GROUP.REMOVE",       cwa_group_remove);

	pn_func(m,  "GROUP.HAS_ADMIN?",   cwa_group_has_admin);
	pn_func(m,  "GROUP.HAS_MEMBER?",  cwa_group_has_member);
	pn_func(m,  "GROUP.RM_ADMIN",     cwa_group_rm_admin);
	pn_func(m,  "GROUP.RM_MEMBER",    cwa_group_rm_member);
	pn_func(m,  "GROUP.ADD_ADMIN",    cwa_group_add_admin);
	pn_func(m,  "GROUP.ADD_MEMBER",   cwa_group_add_member);

	pn_func(m,  "GROUP.GET_GID",      cwa_group_get_gid);
	pn_func(m,  "GROUP.GET_NAME",     cwa_group_get_name);
	pn_func(m,  "GROUP.GET_PASSWD",   cwa_group_get_passwd);
	pn_func(m,  "GROUP.GET_PWHASH",   cwa_group_get_pwhash);

	pn_func(m,  "GROUP.SET_GID",      cwa_group_set_gid);
	pn_func(m,  "GROUP.SET_NAME",     cwa_group_set_name);
	pn_func(m,  "GROUP.SET_PASSWD",   cwa_group_set_passwd);
	pn_func(m,  "GROUP.SET_PWHASH",   cwa_group_set_pwhash);

	pn_func(m,  "AUGEAS.INIT",        cwa_aug_init);
	pn_func(m,  "AUGEAS.SYSERR",      cwa_aug_syserr);
	pn_func(m,  "AUGEAS.SAVE",        cwa_aug_save);
	pn_func(m,  "AUGEAS.CLOSE",       cwa_aug_close);
	pn_func(m,  "AUGEAS.SET",         cwa_aug_set);
	pn_func(m,  "AUGEAS.GET",         cwa_aug_get);
	pn_func(m,  "AUGEAS.FIND",        cwa_aug_find);
	pn_func(m,  "AUGEAS.REMOVE",      cwa_aug_remove);

	pn_func(m,  "EXEC.CHECK",         cwa_exec_check);
	pn_func(m,  "EXEC.RUN1",          cwa_exec_run1);

	pn_func(m,  "SERVER.SHA1",        cwa_server_sha1);
	pn_func(m,  "SERVER.WRITEFILE",   cwa_server_writefile);

	m->U = calloc(1, sizeof(udata));
	m->pragma = cwa_pragma;
	UDATA(m)->zconn = zconn;
	cwa_pragma(m, "userdb.root", "");

	return 0;
}
