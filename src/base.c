/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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

#include "base.h"
#include <assert.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <pwd.h>
#include <grp.h>
#include <fts.h>
#include <utime.h>
#include <pthread.h>
#include <sodium.h>
#include <security/pam_appl.h>
#include <netdb.h>

char* cw_strdup(const char *s)
{
	return (s ? strdup(s) : NULL);
}
int cw_strcmp(const char *a, const char *b)
{
	return ((!a || !b) ? -1 : strcmp(a,b));
}

/*

    ########  ########  ########    ###
    ##     ## ##     ## ##         ## ##
    ##     ## ##     ## ##        ##   ##
    ########  ##     ## ######   ##     ##
    ##     ## ##     ## ##       #########
    ##     ## ##     ## ##       ##     ##
    ########  ########  ##       ##     ##

 */

struct bdfa_hdr {
	char    magic[4];           /* "BDFA" */
	char    flags[4];           /* flags */
	char    mode[8];            /* mode (perms, setuid, etc.) */
	char    uid[8];             /* UID of the file owner */
	char    gid[8];             /* GID of the file group */
	char    mtime[8];           /* modification time */
	char    filesize[8];        /* size of the file */
	char    namesize[8];        /* size of the path + '\0' */
};

static char HEX[16] = "0123456789abcdef";
static uint8_t hexval(char h)
{
	if (h >= '0' && h <= '9') return h - '0';
	if (h >= 'a' && h <= 'f') return h - 'a' + 10;
	if (h >= 'A' && h <= 'F') return h - 'A' + 10;
	return 0;
}

static inline void s_i2c4(char *c4, uint32_t i)
{
	c4[0] = HEX[(i & 0x0000f000) >> 12];
	c4[1] = HEX[(i & 0x00000f00) >>  8];
	c4[2] = HEX[(i & 0x000000f0) >>  4];
	c4[3] = HEX[(i & 0x0000000f) >>  0];
}

static inline uint32_t s_c42i(const char *c4)
{
	return (hexval(c4[0]) << 12)
	     + (hexval(c4[1]) <<  8)
	     + (hexval(c4[2]) <<  4)
	     + (hexval(c4[3]) <<  0);
}

static inline void s_i2c8(char *c8, uint32_t i)
{
	c8[0] = HEX[(i & 0xf0000000) >> 28];
	c8[1] = HEX[(i & 0x0f000000) >> 24];
	c8[2] = HEX[(i & 0x00f00000) >> 20];
	c8[3] = HEX[(i & 0x000f0000) >> 16];
	c8[4] = HEX[(i & 0x0000f000) >> 12];
	c8[5] = HEX[(i & 0x00000f00) >>  8];
	c8[6] = HEX[(i & 0x000000f0) >>  4];
	c8[7] = HEX[(i & 0x0000000f) >>  0];
}

static inline uint32_t s_c82i(const char *c8)
{
	return (hexval(c8[0]) << 28)
	     + (hexval(c8[1]) << 24)
	     + (hexval(c8[2]) << 20)
	     + (hexval(c8[3]) << 16)
	     + (hexval(c8[4]) << 12)
	     + (hexval(c8[5]) <<  8)
	     + (hexval(c8[6]) <<  4)
	     + (hexval(c8[7]) <<  0);
}

int cw_bdfa_pack(int out, const char *root)
{
	int cwd = open(".", O_RDONLY);
	if (cwd < 0)
		return -1;
	if (chdir(root) != 0)
		return -1;

	FTS *fts;
	FTSENT *ent;
	char *paths[2] = { ".", NULL };

	fts = fts_open(paths, FTS_LOGICAL|FTS_XDEV, NULL);
	if (!fts) {
		if (fchdir(cwd) != 0)
			logger(LOG_ERR, "Failed to chdir back to starting directory");
		close(cwd);
		return -1;
	}

	size_t n;
	struct bdfa_hdr h;
	while ( (ent = fts_read(fts)) != NULL ) {
		if (ent->fts_info == FTS_DP) continue;
		if (ent->fts_info == FTS_NS) continue;
		if (strcmp(ent->fts_path, ".") == 0) continue;
		if (!S_ISREG(ent->fts_statp->st_mode)
		 && !S_ISDIR(ent->fts_statp->st_mode)) continue;

		uint32_t filelen = 0;
		uint32_t namelen = strlen(ent->fts_path)-2+1;
		namelen += (4 - (namelen % 4)); /* pad */

		int fd = -1;
		unsigned char *contents = NULL;
		if (S_ISREG(ent->fts_statp->st_mode)) {
			fd = open(ent->fts_accpath, O_RDONLY);
			if (!fd) continue;

			filelen = lseek(fd, 0L, SEEK_END);
			lseek(fd, 0L, SEEK_SET);

			contents = mmap(NULL, namelen, PROT_READ, MAP_SHARED, fd, 0);
			close(fd);
			if (!contents) {
				close(fd);
				continue;
			}
		}

		memset(&h, '0', sizeof(h));
		memcpy(h.magic, "BDFA", 4);
		s_i2c8(h.flags,    0);
		s_i2c8(h.mode,     (uint32_t)ent->fts_statp->st_mode);
		s_i2c8(h.uid,      (uint32_t)ent->fts_statp->st_uid);
		s_i2c8(h.gid,      (uint32_t)ent->fts_statp->st_gid);
		s_i2c8(h.mtime,    (uint32_t)ent->fts_statp->st_mtime);
		s_i2c8(h.filesize, filelen);
		s_i2c8(h.namesize, namelen);
		n = write(out, &h, sizeof(h));
		if (n < sizeof(h))
			logger(LOG_ERR, "short write: %s", strerror(errno));

		char *path = vmalloc(namelen);
		strncpy(path, ent->fts_path+2, namelen);
		n = write(out, path, namelen);
		if (n < namelen)
			logger(LOG_ERR, "short write: %s", strerror(errno));
		free(path);

		if (contents && filelen >= 0) {
			n = write(out, contents, filelen);
			if (n < filelen)
				logger(LOG_ERR, "short write: %s", strerror(errno));
			munmap(contents, filelen);
		}
	}
	fts_close(fts);

	memset(&h, '0', sizeof(h));
	memcpy(h.magic, "BDFA", 4);
	memcpy(h.flags, "0001", 4);
	n = write(out, &h, sizeof(h));
	if (n < sizeof(h))
		logger(LOG_ERR, "short write: %s", strerror(errno));

	if (fchdir(cwd) != 0)
		logger(LOG_ERR, "Failed to chdir back to starting directory");
	close(cwd);
	return 0;
}

int cw_bdfa_unpack(int in, const char *root)
{
	int cwd = open(".", O_RDONLY);
	if (cwd < 0)
		return -1;
	if (root && chdir(root) != 0)
		return -1;

	struct bdfa_hdr h;
	size_t n, len = 0;

	uid_t uid;
	gid_t gid;
	mode_t mode, umsk;
	char *filename;

	int rc = 0;
	umsk = umask(0);
	while ((n = read(in, &h, sizeof(h))) > 0) {
		if (n < sizeof(h)) {
			fprintf(stderr, "Partial read error (only read %li/%li bytes)\n",
				(long)n, (long)sizeof(h));
			rc = 4;
			break;
		}

		if (memcmp(h.flags, "0001", 4) == 0)
			break;

		mode = s_c82i(h.mode);
		uid  = s_c82i(h.uid);
		gid  = s_c82i(h.gid);
		len  = s_c82i(h.namesize);

		filename = vmalloc(len);
		n = read(in, filename, len);

		if (S_ISDIR(mode)) {
			logger(LOG_DEBUG, "BDFA: unpacking directory %s %06o %d:%d",
				filename, mode, uid, gid);

			if (mkdir(filename, mode) != 0 && errno != EEXIST) {
				perror(filename);
				rc = 1;
				continue;
			}
			if (chown(filename, uid, gid) != 0) {
				if (errno != EPERM) {
					perror(filename);
					rc = 1;
					continue;
				}
			}

		} else if (S_ISREG(mode)) {
			len = s_c82i(h.filesize);

			logger(LOG_DEBUG, "BDFA: unpacking file %s %06o %d:%d (%d bytes)",
					filename, mode, uid, gid, len);
			FILE* out = fopen(filename, "w");
			if (!out) {
				perror(filename);
				continue;
			}
			if (fchmod(fileno(out), mode) != 0)
				logger(LOG_ERR, "chmod failed: %s", strerror(errno));
			if (fchown(fileno(out), uid, gid) != 0)
				logger(LOG_ERR, "chown failed: %s", strerror(errno));

			char buf[8192];
			while (len > 0 && (n = read(in, buf, len > 8192 ? 8192: len)) > 0) {
				len -= n;
				fwrite(buf, n, 1, out);
			}
			fclose(out);

		} else {
			fprintf(stderr, "%s - unrecognized mode %08x\n",
				filename, mode);
			continue;
		}

		struct utimbuf ut;
		ut.actime  = s_c82i(h.mtime);
		ut.modtime = s_c82i(h.mtime);

		logger(LOG_DEBUG, "BDFA: setting atime/mtime to %d", ut.modtime);
		if (utime(filename, &ut) != 0) {
			perror(filename);
			rc = 1;
		}

		free(filename);
	}
	umask(umsk);
	if (fchdir(cwd) != 0)
		logger(LOG_ERR, "Failed to chdir back to starting directory");
	close(cwd);
	return rc;
}
/*

     ######  ########  ######## ########   ######
    ##    ## ##     ## ##       ##     ## ##    ##
    ##       ##     ## ##       ##     ## ##
    ##       ########  ######   ##     ##  ######
    ##       ##   ##   ##       ##     ##       ##
    ##    ## ##    ##  ##       ##     ## ##    ##
     ######  ##     ## ######## ########   ######
 */

typedef struct {
	const char *username;
	const char *password;
} _pam_creds_t;

static int s_pam_talker(int n, const struct pam_message **m, struct pam_response **r, void *u)
{
	if (!m || !r || !u) return PAM_CONV_ERR;
	_pam_creds_t *creds = (_pam_creds_t*)u;

	struct pam_response *res = calloc(n, sizeof(struct pam_response));
	if (!res) return PAM_CONV_ERR;

	int i;
	for (i = 0; i < n; i++) {
		res[i].resp_retcode = 0;

		/* the only heuristic that works:
		   PAM_PROMPT_ECHO_ON = asking for username
		   PAM_PROMPT_ECHO_OFF = asking for password
		   */
		switch (m[i]->msg_style) {
		case PAM_PROMPT_ECHO_ON:
			res[i].resp = strdup(creds->username);
			break;
		case PAM_PROMPT_ECHO_OFF:
			res[i].resp = strdup(creds->password);
			break;
		default:
			free(res);
			return PAM_CONV_ERR;
		}
	}
	*r = res;
	return PAM_SUCCESS;
}

static char *_CW_AUTH_ERR = NULL;
int cw_authenticate(const char *service, const char *username, const char *password)
{
	int rc;
	pam_handle_t *pam = NULL;
	_pam_creds_t creds = {
		.username = username,
		.password = password,
	};
	struct pam_conv convo = {
		s_pam_talker,
		(void*)(&creds),
	};

	rc = pam_start(service, creds.username, &convo, &pam);
	if (rc == PAM_SUCCESS)
		rc = pam_authenticate(pam, PAM_DISALLOW_NULL_AUTHTOK);
		if (rc == PAM_SUCCESS)
			rc = pam_acct_mgmt(pam, PAM_DISALLOW_NULL_AUTHTOK);

	if (rc != PAM_SUCCESS) {
		free(_CW_AUTH_ERR);
		_CW_AUTH_ERR = strdup(pam_strerror(pam, rc));
	}

	pam_end(pam, PAM_SUCCESS);
	return rc == PAM_SUCCESS ? 0 : 1;
}

const char *cw_autherror(void)
{
	return _CW_AUTH_ERR ? _CW_AUTH_ERR : "(no error)";
}

int cw_logio(int level, const char *fmt, FILE *io)
{
	char buf[8192];
	for (;;) {
		if (fgets(buf, sizeof(buf), io)) {
			char *nl = strchr(buf, '\n');
			if (nl) *nl = '\0';
			logger(level, fmt, buf);
			continue;
		}
		if (feof(io)) return 0;
		return 1;
	}
}

int cw_fcat(FILE *src, FILE *dst)
{
	assert(src);
	assert(dst);

	char buf[8192]; int n;
	while ((n = fread(buf, 1, 8192, src)) > 0)
		if (fwrite(buf, 1, n, dst) != n)
			return 1;
	return feof(src) ? 0 : 1;
}

int cw_cat(int src, int dst)
{
	assert(src >= 0);
	assert(dst >= 0);

	char buf[8192]; int n;
	while ((n = read(src, buf, 8192)) > 0)
		if (write(dst, buf, n) != n)
			return 1;
	return n;
}

char* cw_escape_json(const char *in)
{
	size_t n = 0;
	char *esc, *a;
	const char *b = in;

	while (*b) {
		n++;
		switch (*b) {
		case '\\':
		case '"' :
		case '\n':
		case '\r':
		case '\t':
			n++;
		}
		b++;
	}

	a = esc = vcalloc(n + 1, sizeof(char));
	if (!esc) return NULL;

	while (*in) {
		switch (*in) {
		case '\\':
		case '"' :
		case '\n':
		case '\r':
		case '\t':
			*a++ = '\\';
		}

		switch (*in) {
		case '\n': *a++ = 'n'; in++; break;
		case '\r': *a++ = 'r'; in++; break;
		case '\t': *a++ = 't'; in++; break;
		default:
			*a++ = *in++;
		}
	}
	*a = '\0';
	return esc;
}

char* cw_escape_cdata(const char *in)
{
	char *esc, *a;
	const char *needle = in;
	int n = 1;
	while ((needle = strstr(needle, "]]>"))) {
		needle += 2; /* skip ']]' */
		n++;
	}
	n *= strlen("<![CDATA[]]>");

	a = esc = vcalloc(strlen(in) + n + 1, sizeof(char));
	if (!esc) return NULL;
	while (in) {
		memcpy(a, "<![CDATA[", strlen("<![CDATA[")+1);
		a += strlen("<![CDATA[");

		needle = strstr(in, "]]>");
		if (needle) {
			needle += 2; /* skip ']]' */

			memcpy(a, in, needle - in);
			a += needle - in;

		} else {
			memcpy(a, in, strlen(in));
			a += strlen(in);
		}
		in = needle;

		memcpy(a, "]]>", strlen("]]>"));
		a += strlen("]]>");
	}

	return esc;
}

