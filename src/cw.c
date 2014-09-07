#include "cw.h"
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

/*

    ##     ## ######## ##     ##
    ###   ### ##       ###   ###
    #### #### ##       #### ####
    ## ### ## ######   ## ### ##
    ##     ## ##       ##     ##
    ##     ## ##       ##     ##
    ##     ## ######## ##     ##

 */

void* __cw_alloc(size_t size, const char *func, const char *file, unsigned int line)
{
	void *buf = calloc(1, size);
	if (buf) return buf;

	cw_log(LOG_CRIT, "%s, %s:%u - malloc failed: %s",
	      func, file, line, strerror(errno));
	exit(42);
}

char* cw_strdup(const char *s)
{
	return (s ? strdup(s) : NULL);
}
int cw_strcmp(const char *a, const char *b)
{
	return ((!a || !b) ? -1 : strcmp(a,b));
}

char** cw_arrdup(char **a)
{
	char **n, **t;

	if (!a) { return NULL; }
	for (t = a; *t; t++)
		;

	n = cw_alloc((t -a + 1) * sizeof(char*));
	for (t = n; *a; a++)
		*t++ = cw_strdup(*a);

	return n;
}

void cw_arrfree(char **a)
{
	char **s;
	if (!a) { return; }
	for (s = a; *s; free(*s++));
	free(a);
}

/*

    ########     ###     ######  ########
    ##     ##   ## ##   ##    ## ##        ##   ##
    ##     ##  ##   ##  ##       ##         ## ##
    ########  ##     ##  ######  ######   #########
    ##     ## #########       ## ##         ## ##
    ##     ## ##     ## ##    ## ##        ##   ##
    ########  ##     ##  ######  ########

 */

static char    BASE16_ENCODE[16] = { "0123456789abcdef" };
static uint8_t BASE16_DECODE[256] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 000 (NUL) - 007 (BEL) */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 010 (BS)  - 017 (SI)  */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 020 (DLE) - 027 (ETB) */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 030 (CAN) - 037 (US)  */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 040 ( )   - 047 (')   */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 050 (()   - 057 (/)   */

	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, /* '0' .. '7' */
	0x08, 0x09,                                     /* '8' .. '9' */

	            0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 072 (:)   - 077 (?)   */
	0xff,                                           /* 100 (@)               */

	      0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,       /* 'A' .. 'F' */

	                                          0xff, /*           - 107 (G)   */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 110 (H)   - 117 (O)   */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 120 (P)   - 127 (W)   */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 130 (X)   - 137 (_)   */
	0xff,                                           /* 140 (`)               */

	      0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,       /* 'a' .. 'f' */
	                                          0xff, /*           - 147 (g)   */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 150 (h)   - 157 (o)   */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 160 (p)   - 167 (w)   */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 170 (x)   - 177 (DEL) */

	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 200 - 207 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 210 - 217 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 220 - 227 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 230 - 237 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 240 - 247 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 250 - 257 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 260 - 267 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 270 - 277 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 300 - 307 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 310 - 317 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 320 - 327 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 330 - 337 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 340 - 347 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 350 - 357 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 360 - 367 */
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* 370 - 377 */
};

int base16_encode(char *dst, size_t dlen, const void *src, size_t slen)
{
	/* empty string encodes as itself (no bytes) */
	if (slen == 0) return 0;

	assert(dst);
	assert(src);

	errno = EINVAL;
	if (dlen < slen * 2) return -1;

	char *d = dst;
	int i;
	for (i = 0; i < slen; i++) {
		*d++ = BASE16_ENCODE[((((uint8_t*)src)[i]) & 0xf0) >> 4];
		*d++ = BASE16_ENCODE[((((uint8_t*)src)[i]) & 0x0f)];
	}

	return d - dst;
}

int base16_decode(void *dst, size_t dlen, const char *src, size_t slen)
{
	/* empty string decodes as itself (no bytes) */
	if (slen == 0) return 0;

	assert(dst);
	assert(src);

	errno = EINVAL;
	assert(slen > 0);
	if (dlen < slen / 2) return -1;

	void *d = dst;
	int i;
	errno = EILSEQ;
	for (i = 0; i < slen; i += 2) {
		uint8_t hi, lo;
		hi = BASE16_DECODE[(uint8_t)src[i  ]];
		lo = BASE16_DECODE[(uint8_t)src[i+1]];
		if (hi == 0xff || lo == 0xff) return -1;

		*(uint8_t*)d++ = (hi << 4) + lo;
	}

	return d - dst;
}

char* base16_encodestr(const void *src, size_t len)
{
	assert(src);
	errno = EINVAL;
	if (len <= 0) return NULL;

	size_t dlen = len * 2;
	char *dst = cw_alloc(sizeof(char) * (dlen + 1));

	int rc = base16_encode(dst, dlen, src, len);
	if (rc < 0) {
		free(dst);
		return NULL;
	}

	dst[rc] = '\0';
	return dst;
}

char* base16_decodestr(const char *src, size_t len)
{
	assert(src);
	errno = EINVAL;
	if (len <= 0) return NULL;

	size_t dlen = len / 2;
	char *dst = cw_alloc(sizeof(char) * (dlen + 1));

	int rc = base16_decode(dst, dlen, src, len);
	if (rc < 0) {
		free(dst);
		return NULL;
	}

	dst[rc] = '\0';
	return dst;
}

/*

     ######  ##        #######   ######  ##    ##
    ##    ## ##       ##     ## ##    ## ##   ##
    ##       ##       ##     ## ##       ##  ##
    ##       ##       ##     ## ##       #####
    ##       ##       ##     ## ##       ##  ##
    ##    ## ##       ##     ## ##    ## ##   ##
     ######  ########  #######   ######  ##    ##

 */
void cw_timer_start(cw_timer_t *clock)
{
	gettimeofday(&clock->tv, NULL);
	clock->running = 1;
}

void cw_timer_stop(cw_timer_t *clock)
{
	clock->running = 0;
	struct timeval end, diff;
	if (gettimeofday(&end, NULL) != 0)
		return;

	if ((end.tv_usec - clock->tv.tv_usec) < 0) {
		diff.tv_sec  = end.tv_sec - clock->tv.tv_sec - 1;
		diff.tv_usec = 1000000 + end.tv_usec-clock->tv.tv_usec;
	} else {
		diff.tv_sec  = end.tv_sec-clock->tv.tv_sec;
		diff.tv_usec = end.tv_usec-clock->tv.tv_usec;
	}

	clock->tv.tv_sec  = diff.tv_sec;
	clock->tv.tv_usec = diff.tv_usec;
}

uint32_t cw_timer_s(const cw_timer_t *clock)
{
	return clock->tv.tv_sec;
}

uint64_t cw_timer_ms(const cw_timer_t *clock)
{
	return clock->tv.tv_sec  * 1000
	     + clock->tv.tv_usec / 1000;
}

/*
     ######  ####  ######   ##    ##    ###    ##        ######
    ##    ##  ##  ##    ##  ###   ##   ## ##   ##       ##    ##
    ##        ##  ##        ####  ##  ##   ##  ##       ##
     ######   ##  ##   #### ## ## ## ##     ## ##        ######
          ##  ##  ##    ##  ##  #### ######### ##             ##
    ##    ##  ##  ##    ##  ##   ### ##     ## ##       ##    ##
     ######  ####  ######   ##    ## ##     ## ########  ######
 */
static int CW_SIGNALLED = 0;
static void cw_sig_handler(int sig)
{
	CW_SIGNALLED = 1;
}

void cw_sig_catch(void)
{
	struct sigaction action;
	action.sa_handler = cw_sig_handler;
	action.sa_flags = SA_SIGINFO;

	sigemptyset(&action.sa_mask);
	sigaction(SIGINT,  &action, NULL);
	sigaction(SIGTERM, &action, NULL);
}

int cw_sig_interrupt(void)
{
	return CW_SIGNALLED > 0;
}

/*
    ##       ####  ######  ########  ######
    ##        ##  ##    ##    ##    ##    ##
    ##        ##  ##          ##    ##
    ##        ##   ######     ##     ######
    ##        ##        ##    ##          ##
    ##        ##  ##    ##    ##    ##    ##
    ######## ####  ######     ##     ######
 */

int cw_list_init(cw_list_t *l)
{
	assert(l);
	l->next = l->prev = l;
	return 0;
}

int cw_list_isempty(cw_list_t *l)
{
	assert(l);
	return l->next == l;
}

size_t cw_list_len(cw_list_t *lst)
{
	assert(lst);
	size_t len = 0;
	cw_list_t *n;

	for_each(n, lst)
		len++;

	return len;
}

int cw_list_splice(cw_list_t *prev, cw_list_t *next)
{
	assert(prev);
	assert(next);

	next->prev = prev;
	prev->next = next;
	return 0;
}

int cw_list_delete(cw_list_t *n)
{
	assert(n);
	return cw_list_splice(n->prev, n->next) == 0
	    && cw_list_init(n)                  == 0 ? 0 : -1;
}

int cw_list_replace(cw_list_t *o, cw_list_t *n)
{
	assert(o);
	assert(n);

	n->next = o->next;
	n->next->prev = n;

	n->prev = o->prev;
	n->prev->next = n;

	cw_list_init(o);
	return 0;
}

int cw_list_unshift(cw_list_t *l, cw_list_t *n)
{
	assert(l);
	assert(n);

	return cw_list_splice(n, l->next) == 0
	    && cw_list_splice(l, n)       == 0 ? 0 : -1;
}

int cw_list_push(cw_list_t *l, cw_list_t *n)
{
	assert(l);
	assert(n);

	return cw_list_splice(l->prev, n) == 0
	    && cw_list_splice(n,       l) == 0 ? 0 : -1;
}

cw_list_t *cw_list_shift(cw_list_t *l)
{
	assert(l);
	if (cw_list_isempty(l))
		return NULL;

	cw_list_t *n = l->next;
	if (cw_list_splice(l, n->next) != 0)
		return NULL;
	return n;
}

cw_list_t *cw_list_pop(cw_list_t *l)
{
	assert(l);
	if (cw_list_isempty(l))
		return NULL;

	cw_list_t *n = l->prev;
	if (cw_list_splice(n->prev, l) != 0)
		return NULL;
	return n;
}

/*
     ######  ######## ########  #### ##    ##  ######    ######
    ##    ##    ##    ##     ##  ##  ###   ## ##    ##  ##    ##
    ##          ##    ##     ##  ##  ####  ## ##        ##
     ######     ##    ########   ##  ## ## ## ##   ####  ######
          ##    ##    ##   ##    ##  ##  #### ##    ##        ##
    ##    ##    ##    ##    ##   ##  ##   ### ##    ##  ##    ##
     ######     ##    ##     ## #### ##    ##  ######    ######
 */

char* cw_string(const char *fmt, ...)
{
	char buf[256], *s;
	va_list args;
	va_start(args, fmt);
	size_t n = vsnprintf(buf, 256, fmt, args) + 1;
	va_end(args);

	if (n > 256) {
		s = cw_alloc(n * sizeof(char));

		va_start(args, fmt);
		vsnprintf(s, n, fmt, args);
		va_end(args);
		return s;
	}

	return cw_strdup(buf);
}

#define CW_INTERPOLATE_COPY 0
#define CW_INTERPOLATE_SREF 1 /* "simple" reference:  $([a-zA-Z][a-zA-Z0-9]*) */
#define CW_INTERPOLATE_CREF 2 /* "complex" reference: ${([a-zA-Z][^}]*)} */
#define CW_INTERPOLATE_ESC  3 /* a leading '\' for escaping '$' */

#define s_si_putc(b,c,l) do {\
	if ((l) > 0) { (l)--; *(b)++ = (c); } \
} while(0)

static int s_si_count(const char *start, const char *end, const cw_hash_t *vars)
{
	char *var = cw_alloc((end - start) + 1);
	if (!strncpy(var, start, end - start)) {
		free(var);
		return 0;
	}

	const char *val = cw_hash_get(vars, var);
	free(var);

	return val ? strlen(val) : 0;
}

static int s_si_deref(char **buf, size_t *len, const char *start, const char *end, const cw_hash_t *vars)
{
	char *var = cw_alloc((end - start) + 1);
	if (!strncpy(var, start, end - start)) {
		free(var);
		return 1;
	}

	const char *val = cw_hash_get(vars, var);
	if (!val) val = "";
	free(var);

	strncat(*buf, val, *len);
	for (; **buf; (*buf)++, (*len)--)
		;
	return 0;
}

char* cw_interpolate(const char *str, cw_hash_t *vars)
{
	size_t len = 0;

	int state = CW_INTERPOLATE_COPY;
	const char *ref = NULL;
	const char *s = str;

	while (*s) {
		if (state == CW_INTERPOLATE_SREF) {
			if (!isalnum(*s)) {
				state = CW_INTERPOLATE_COPY;
				len += s_si_count(ref, s, vars);
			}
		} else if (state == CW_INTERPOLATE_CREF) {
			if (*s == '}') {
				state = CW_INTERPOLATE_COPY;
				len += s_si_count(ref, s, vars);
				s++;
			}
		}

		if (state == CW_INTERPOLATE_ESC) {
			state = CW_INTERPOLATE_COPY;
			len++; s++;
			continue;
		}

		if (state == CW_INTERPOLATE_COPY) {
			if (*s == '\\') {
				state = CW_INTERPOLATE_ESC;
				s++;
				continue;

			} else if (*s == '$') {
				state = CW_INTERPOLATE_SREF;
				s++;

				if (*s && *s == '{') {
					state = CW_INTERPOLATE_CREF;
					s++;
				}

				ref = s;
				continue;
			}

			len++;
		}
		s++;
	}

	if (state != CW_INTERPOLATE_COPY
	 && state != CW_INTERPOLATE_ESC)
		len += s_si_count(ref, s, vars);

	char *final = cw_alloc(len+1);
	char *buf = final;

	state = CW_INTERPOLATE_COPY;
	ref = NULL;
	s = str;

	while (*s && len > 0) {
		if (state == CW_INTERPOLATE_SREF) {
			if (!isalnum(*s)) {
				state = CW_INTERPOLATE_COPY;
				s_si_deref(&buf, &len, ref, s, vars);
			}
		} else if (state == CW_INTERPOLATE_CREF) {
			if (*s == '}') {
				state = CW_INTERPOLATE_COPY;
				s_si_deref(&buf, &len, ref, s++, vars);
			}
		}

		if (state == CW_INTERPOLATE_ESC) {
			state = CW_INTERPOLATE_COPY;
			s_si_putc(buf, *s++, len);
			continue;
		}

		if (state == CW_INTERPOLATE_COPY) {
			if (*s == '\\') {
				state = CW_INTERPOLATE_ESC;
				s++;
				continue;

			} else if (*s == '$') {
				state = CW_INTERPOLATE_SREF;
				s++;

				if (*s && *s == '{') {
					state = CW_INTERPOLATE_CREF;
					s++;
				}

				ref = s;
				continue;
			}

			s_si_putc(buf, *s, len);
		}

		s++;
	}

	if (state != CW_INTERPOLATE_COPY
	 && state != CW_INTERPOLATE_ESC)
		s_si_deref(&buf, &len, ref, s, vars);
	*buf = '\0';

	return final;
}

/*
    ##    ##     ###     ######   ##    ##  ########  ######
    ##    ##    ## ##   ##    ##  ##    ##  ##       ##    ##
    ##    ##   ##   ##  ##        ##    ##  ##       ##
    ########  ##     ##  ######   ########  ######    ######
    ##    ##  #########       ##  ##    ##  ##             ##
    ##    ##  ##     ## ##    ##  ##    ##  ##       ##    ##
    ##    ##  ##     ##  ######   ##    ##  ########  ######
 */
static uint8_t s_hash64(const char *s)
{
	unsigned int h = 81;
	unsigned char c;

	while ((c = *s++))
		h = ((h << 5) + h) + c;

	return h & ~0xc0;
}

static ssize_t s_hash_index(const struct cw_hash_bkt *b, const char *k)
{
	ssize_t i;
	for (i = 0; i < b->len; i++)
		if (strcmp(b->keys[i], k) == 0)
			return i;
	return (ssize_t)-1;
}
static int s_hash_insert(struct cw_hash_bkt *b, const char *k, void *v)
{
	if (!k) return 1;

	char ** new_k = realloc(b->keys,   (b->len + 1) * sizeof(char*));
	char ** new_v = realloc(b->values, (b->len + 1) * sizeof(void*));

	new_k[b->len] = strdup(k);
	new_v[b->len] = v;

	b->keys   = new_k;
	b->values = new_v;
	b->len++;

	return 0;
}

int cw_hash_done(cw_hash_t *h, uint8_t all)
{
	ssize_t i, j;
	if (h) {
		for (i = 0; i < 64; i++) {
			for (j = 0; j < h->entries[i].len; j++) {
				free(h->entries[i].keys[j]);
				if (all) free(h->entries[i].values[j]);
			}
			free(h->entries[i].keys);
			free(h->entries[i].values);
		}
	}
	return 0;
}

void* cw_hash_get(const cw_hash_t *h, const char *k)
{
	if (!h || !k) return NULL;

	const struct cw_hash_bkt *b = &h->entries[s_hash64(k)];
	ssize_t i = s_hash_index(b, k);
	return (i < 0 ? NULL : b->values[i]);
}

void* cw_hash_set(cw_hash_t *h, const char *k, void *v)
{
	if (!h || !k) return NULL;

	struct cw_hash_bkt *b = &h->entries[s_hash64(k)];
	ssize_t i = s_hash_index(b, k);

	if (i < 0) {
		s_hash_insert(b, k, v);
		return v;
	}

	void *existing = b->values[i];
	b->values[i] = v;
	return existing;
}

void* cw_hash_next(cw_hash_t *h, char **k, void **v)
{
	assert(h); // LCOV_EXCL_LINE

	char *tmp = NULL;
	if (k) *k = NULL;
	if (v) *v = NULL;
	while (h->bucket < 64) {
		struct cw_hash_bkt *b = h->entries + h->bucket;
		if (h->offset >= b->len) {
			h->bucket++;
			h->offset = 0;
			continue;
		}
		tmp = b->keys[h->offset];
		if (k) *k = b->keys[h->offset];
		if (v) *v = b->values[h->offset];
		h->offset++;
		break;
	}
	return tmp;
}

int cw_hash_merge(cw_hash_t *a, cw_hash_t *b)
{
	assert(a); // LCOV_EXCL_LINE;
	assert(b); // LCOV_EXCL_LINE;

	char *k; void *v;
	for_each_key_value(b, k, v)
		cw_hash_set(a, k, v);
	return 0;
}

/*
    ######## #### ##     ## ########
       ##     ##  ###   ### ##
       ##     ##  #### #### ##
       ##     ##  ## ### ## ######
       ##     ##  ##     ## ##
       ##     ##  ##     ## ##
       ##    #### ##     ## ########
 */

int32_t cw_time_s(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec;
}

int64_t cw_time_ms(void)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return (int64_t) ((int64_t) tv.tv_sec * 1000 + (int64_t) tv.tv_usec / 1000);
}

const char *cw_time_strf(const char *fmt, int32_t s)
{
	static char buf[1024];

	time_t ts = (time_t)s;
	struct tm tm;
	if (!localtime_r(&ts, &tm))
		return NULL;

	if (!fmt) fmt = "%x %X";
	size_t n = strftime(buf, sizeof(buf)-1, fmt, &tm);
	buf[n] = '\0';
	return buf;
}

int cw_sleep_ms(int64_t ms)
{
	struct timespec ts;
	ts.tv_sec  = (time_t)(ms / 1000.0);
	ts.tv_nsec = (long)((ms - (ts.tv_sec * 1000)) * 1000.0 * 1000.0);
	return nanosleep(&ts, NULL);
}

/*
    ########     ###    ##    ## ########
    ##     ##   ## ##   ###   ## ##     ##
    ##     ##  ##   ##  ####  ## ##     ##
    ########  ##     ## ## ## ## ##     ##
    ##   ##   ######### ##  #### ##     ##
    ##    ##  ##     ## ##   ### ##     ##
    ##     ## ##     ## ##    ## ########
 */

static int CW_RAND_SEED = 0;
void cw_srand(void)
{
	if (CW_RAND_SEED)
		return;
	srand(CW_RAND_SEED = cw_time_ms());
}

/*
    ######## ##     ##  #######
         ##  ###   ### ##     ##
        ##   #### #### ##     ##
       ##    ## ### ## ##     ##
      ##     ##     ## ##  ## ##
     ##      ##     ## ##    ##
    ######## ##     ##  ##### ##
 */

#define rnd(num) ((int)((float)(num) * random() / (RAND_MAX + 1.0)))
void * cw_zmq_ident(void *zocket, void *buf)
{
	char *id = (char*)buf;
	if (!id) {
		id = malloc(8);
		assert(id);

		cw_srand();
		id[0] = rnd(256); id[1] = rnd(256);
		id[2] = rnd(256); id[3] = rnd(256);
		id[4] = rnd(256); id[5] = rnd(256);
		id[6] = getpid() & 0xff;
		id[7] = getpid() >> 8;
	}
	zmq_setsockopt(zocket, ZMQ_IDENTITY, id, 8);
	return id;
}

static cw_frame_t * s_cw_frame_build(cw_frame_t *f)
{
	assert(f);

	f->size = zmq_msg_size(&f->msg);
	f->data = calloc(f->size+1, sizeof(char));
	assert(f->data);
	assert(zmq_msg_data(&f->msg));
	memcpy(f->data, zmq_msg_data(&f->msg), f->size);

	size_t i;
	for (i = 0; i < f->size; i++) {
		if (!isprint(f->data[i])) {
			f->binary = 1;
			break;
		}
	}

	return f;
}

cw_frame_t *cw_frame_recv(void *zocket)
{
	assert(zocket);

	cw_frame_t *f = calloc(1, sizeof(cw_frame_t));
	assert(f);

	int rc = zmq_msg_init(&f->msg);
	assert(rc == 0);

	if (zmq_msg_recv(&f->msg, zocket, 0) == -1) {
		zmq_msg_close(&f->msg);
		free(f);
		return NULL;
	}

	size_t len = sizeof(f->more);
	rc = zmq_getsockopt(zocket, ZMQ_RCVMORE, &f->more, &len);
	if (rc != 0) perror("zmq_getsockopt");
	assert(rc == 0);

	return s_cw_frame_build(f);
}

int cw_frame_send(void *zocket, cw_frame_t *f)
{
	return zmq_msg_send(&f->msg, zocket,
			f->more ? ZMQ_SNDMORE : 0);
}

cw_frame_t *cw_frame_new(const char *s)
{
	assert(s);

	cw_frame_t *f = calloc(1, sizeof(cw_frame_t));
	assert(f);

	size_t len = strlen(s);
	int rc = zmq_msg_init_size(&f->msg, len);
	assert(rc == 0);

	memcpy(zmq_msg_data(&f->msg), s, len);

	return s_cw_frame_build(f);
}

cw_frame_t *cw_frame_newbuf(const char *buf, size_t len)
{
	assert(buf);

	cw_frame_t *f = calloc(1, sizeof(cw_frame_t));
	assert(f);

	int rc = zmq_msg_init_size(&f->msg, len);
	assert(rc == 0);

	memcpy(zmq_msg_data(&f->msg), buf, len);

	return s_cw_frame_build(f);
}

cw_frame_t *cw_frame_copy(cw_frame_t *f)
{
	cw_frame_t *new = calloc(1, sizeof(cw_frame_t));
	assert(new);

	zmq_msg_init_size(&new->msg, f->size);
	memcpy(&new->msg, &f->msg, f->size);
	return s_cw_frame_build(new);
}

void cw_frame_close(cw_frame_t *f)
{
	f->size = 0;
	free(f->data);
	f->data = NULL;
	f->binary = 0;
	zmq_msg_close(&f->msg);
}

char *cw_frame_text(cw_frame_t *f)
{
	if (!f)
		return NULL;

	char *s = calloc(f->size + 1, sizeof(char));
	assert(s);
	memcpy(s, f->data, f->size);
	return s;
}

char *cw_frame_hex(cw_frame_t *f)
{
	assert(f);

	static const char hex[] = "0123456789abcdef";
	size_t i;

	char *s = calloc(2 * f->size + 1, sizeof(char));
	assert(s);

	for (i = 0; i < f->size; i++) {
		s[i * 2 + 0] = hex[f->data[i] >> 4];
		s[i * 2 + 1] = hex[f->data[i] & 0x0f];
	}
	s[i * 2] = '\0';
	return s;
}

int cw_frame_is(cw_frame_t *f, const char *s)
{
	assert(f);
	assert(s);

	return memcmp(f->data, s, strlen(s)) == 0;
}

int cw_frame_cmp(cw_frame_t *a, cw_frame_t *b)
{
	assert(a);
	assert(b);

	size_t len = a->size > b->size ? a->size : b->size;
	return memcmp(a->data, b->data, len) == 0;
}

void cw_frame_dump(FILE *io, cw_frame_t *f)
{
	fprintf(io, "[%i] ", (int)(f->size));

	size_t len = f->size;
	size_t max = f->binary ? 35 : 70;
	const char *ellips = "";
	if (len > max) {
		len = max;
		ellips = "...";
	}

	size_t i;
	for (i = 0; i < len; i++)
		fprintf(stderr, (f->binary ? "%02x" : "%c"), (f->data[i]));
	fprintf(stderr, "%s%s\n", ellips,
		(f->more ? " (+)" : ""));
}

zmq_msg_t * cw_zmq_msg_recv(void *zocket, zmq_msg_t *msg, int *more)
{
	assert(zocket);
	assert(more);
	assert(msg);

	if (zmq_msg_recv(msg, zocket, 0) != -1) {
		size_t len = sizeof(int);
		int rc = zmq_getsockopt(zocket, ZMQ_RCVMORE, more, &len);
		assert(rc == 0);

		return msg;
	}

	free(msg);
	return NULL;
}

void cw_zmq_shutdown(void *zocket, int linger)
{
	if (linger < 0) linger = 500;
	int rc = zmq_setsockopt(zocket, ZMQ_LINGER, &linger, sizeof(linger));
	if (rc != 0)
		cw_log(LOG_ERR, "failed to set ZMQ_LINGER to %i on socket %p: %s",
			linger, zocket, zmq_strerror(errno));
	assert(rc == 0);

	rc = zmq_close(zocket);
	assert(rc == 0);
}

int cw_pdu_init(cw_pdu_t *pdu)
{
	assert(pdu);

	pdu->client = NULL;
	pdu->type   = NULL;
	pdu->data   = NULL;

	pdu->src = NULL;
	pdu->len = 0;
	cw_list_init(&pdu->frames);

	return 0;
}

void cw_pdu_destroy(cw_pdu_t *pdu)
{
	if (!pdu)
		return;

	free(pdu->client);
	free(pdu->type);
	free(pdu->data);

	cw_frame_t *f, *tmp;
	for_each_object_safe(f, tmp, &pdu->frames, l) {
		cw_list_delete(&f->l);
		cw_frame_close(f);
		free(f);
	}

	if (pdu->src) {
		cw_frame_close(pdu->src);
		free(pdu->src);
	}

	free(pdu);
}

void cw_pdu_dump(FILE *io, cw_pdu_t *pdu)
{
	fprintf(io, "-----------------------------------\n");
	if (pdu->src) {
		cw_frame_dump(io, pdu->src);
		fprintf(io, "---\n");
	}
	cw_frame_t *f;
	for_each_object(f, &pdu->frames, l)
		cw_frame_dump(io, f);
	fprintf(io, "...\n");
	fprintf(io, "client: |%s|\n", pdu->client);
	fprintf(io, "type:   |%s|\n", pdu->type);
	fprintf(io, "data:   |%s|\n", pdu->data);
	fprintf(io, "EOF\n");
}

cw_frame_t *cw_pdu_frame(cw_pdu_t *pdu, size_t n)
{
	assert(pdu);
	assert(n >= 0);
	if (n >= pdu->len)
		return NULL;

	cw_frame_t *f = cw_list_head(&pdu->frames, cw_frame_t, l);
	while (n-- > 0)
		f = cw_list_next(f, l);
	return f;
}

size_t cw_pdu_framelen(cw_pdu_t *pdu, size_t n)
{
	cw_frame_t *f = cw_pdu_frame(pdu, n);
	return f ? f->size : 0;
}

static cw_pdu_t * s_cw_pdu_new(void)
{
	cw_pdu_t *pdu = malloc(sizeof(cw_pdu_t));
	assert(pdu);

	int rc = cw_pdu_init(pdu);
	assert(rc == 0);

	pdu->len = 0;
	pdu->src = NULL;

	return pdu;
}

cw_pdu_t *cw_pdu_recv(void *zocket)
{
	cw_pdu_t *pdu = s_cw_pdu_new();
	assert(pdu);

	int body = 0;
	cw_frame_t *f;
	for (;;) {
		f = cw_frame_recv(zocket);
		if (!f) {
			cw_pdu_destroy(pdu);
			return NULL;
		}

		if (!body) {
			if (f->size == 0) {
				body = 1;
				int more = f->more;
				cw_frame_close(f);
				free(f);

				if (more) continue;
				else break;
			} else {
				assert(!pdu->src);
				pdu->src = f;
			}
		} else {
			int rc = cw_pdu_extend(pdu, f);
			assert(rc == 0);
		}

		if (!f->more)
			break;
	}

	free(pdu->client);
	pdu->client = pdu->src ? cw_frame_hex(pdu->src) : strdup("none");

	free(pdu->type); pdu->type = cw_pdu_text(pdu, 0);
	if (!pdu->type)  pdu->type = strdup("NOOP");

	free(pdu->data); pdu->data = cw_pdu_text(pdu, 1);
	if (!pdu->data)  pdu->data = strdup(".");

	return pdu;
}

cw_pdu_t *cw_pdu_make(cw_frame_t *dest, int n, ...)
{
	cw_pdu_t *pdu = s_cw_pdu_new();
	assert(pdu);

	if (dest)
		pdu->src = cw_frame_copy(dest);

	va_list ap;
	va_start(ap, n);
	while (n-- > 0)
		cw_pdu_extend(pdu, cw_frame_new(va_arg(ap, char *)));
	va_end(ap);

	free(pdu->type); pdu->type = cw_pdu_text(pdu, 0);
	if (!pdu->type)  pdu->type = strdup("NOOP");
	return pdu;
}

int cw_pdu_extend(cw_pdu_t *pdu, cw_frame_t *f)
{
	assert(pdu);
	assert(f);

	cw_list_push(&pdu->frames, &f->l);
	pdu->len++;
	return 0;
}

int cw_pdu_send(void *zocket, cw_pdu_t *pdu)
{
	int rc;
	cw_frame_t *f;

	//cw_pdu_dump(stderr, pdu);
	if (pdu->src) {
		pdu->src->more = 1;
		rc = cw_frame_send(zocket, pdu->src);
		assert(rc >= 0);
	}

	f = cw_frame_new("");
	f->more = 1;
	rc = cw_frame_send(zocket, f);
	assert(rc >= 0);
	cw_frame_close(f);
	free(f);

	for_each_object(f, &pdu->frames, l) {
		f->more = (f->l.next == &pdu->frames ? 0 : 1);
		rc = cw_frame_send(zocket, f);
		assert(rc >= 0);
	}

	return 0;
}

typedef struct {
	cw_list_t   l;
	void       *socket;
	reactor_fn  fn;
	void       *data;
	int         index;
} cw_reactor_item_t;

cw_reactor_t *cw_reactor_new(void)
{
	cw_reactor_t *r = cw_alloc(sizeof(cw_reactor_t));
	cw_list_init(&r->reactors);
	return r;
}

void cw_reactor_destroy(cw_reactor_t *r)
{
	if (!r) free(r);

	cw_reactor_item_t *item, *tmp;
	for_each_object_safe(item, tmp, &r->reactors, l)
		free(item);
	free(r->poller);
	free(r);
}

int cw_reactor_add(cw_reactor_t *r, void *socket, reactor_fn fn, void *data)
{
	assert(r);
	assert(socket);
	assert(fn);

	cw_reactor_item_t *item = cw_alloc(sizeof(cw_reactor_item_t));
	cw_list_init(&item->l);
	item->socket = socket;
	item->fn     = fn;
	item->data   = data;

	cw_list_push(&r->reactors, &item->l);

	size_t n = cw_list_len(&r->reactors);
	free(r->poller);
	r->poller = cw_alloc(n * sizeof(zmq_pollitem_t));

	n = 0;
	for_each_object(item, &r->reactors, l) {
		item->index = n;
		r->poller[n].socket  = item->socket;
		r->poller[n].events  = ZMQ_POLLIN;
		n++;
	}

	return 0;
}

int cw_reactor_loop(cw_reactor_t *r)
{
	assert(r);
	int rc;

	size_t n = cw_list_len(&r->reactors);
	while ( (rc = zmq_poll(r->poller, n, -1)) >= 0) {
		cw_reactor_item_t *item;
		for_each_object(item, &r->reactors, l) {
			if (r->poller[item->index].revents != ZMQ_POLLIN)
				continue;

			cw_pdu_t *pdu = cw_pdu_recv(item->socket);
			if (!pdu) continue;

			rc = (*(item->fn))(item->socket, pdu, item->data);
			cw_pdu_destroy(pdu);

			if (rc != CW_REACTOR_CONTINUE) return 0;
		}
	}

	return 1;
}

/*
     ######   #######  ##    ## ######## ####  ######
    ##    ## ##     ## ###   ## ##        ##  ##    ##
    ##       ##     ## ####  ## ##        ##  ##
    ##       ##     ## ## ## ## ######    ##  ##   ####
    ##       ##     ## ##  #### ##        ##  ##    ##
    ##    ## ##     ## ##   ### ##        ##  ##    ##
     ######   #######  ##    ## ##       ####  ######
 */

int cw_cfg_set(cw_list_t *cfg, const char *key, const char *val)
{
	cw_keyval_t *kv;
	for_each_object(kv, cfg, l) {
		if (strcmp(kv->key, key) != 0)
			continue;
		free(kv->val);
		kv->val = strdup(val);
		return 0;
	}

	kv = malloc(sizeof(cw_keyval_t));
	assert(kv);

	kv->key = strdup(key);
	kv->val = strdup(val);
	cw_list_unshift(cfg, &kv->l);
	return 0;
}

int cw_cfg_unset(cw_list_t *cfg, const char *key)
{
	cw_keyval_t *kv, *tmp;
	for_each_object_safe(kv, tmp, cfg, l) {
		if (strcmp(kv->key, key) != 0)
			continue;
		cw_list_delete(&kv->l);
		free(kv->key);
		free(kv->val);
		free(kv);
	}
	return 0;
}

char * cw_cfg_get(cw_list_t *cfg, const char *key)
{
	cw_keyval_t *kv;
	for_each_object(kv, cfg, l)
		if (strcmp(kv->key, key) == 0)
			return kv->val;
	return NULL;
}

int cw_cfg_isset(cw_list_t *cfg, const char *key)
{
	cw_keyval_t *kv;
	for_each_object(kv, cfg, l)
		if (strcmp(kv->key, key) == 0)
			return 1;
	return 0;
}

int cw_cfg_read(cw_list_t *cfg, FILE *io)
{
	cw_keyval_t *kv;
	char line[8192];
	while (fgets(line, 8191, io)) {
		/* FIXME: doesn't handle large lines (>8192) */
		/*
		   "   directive    value  \n"
		    ^  ^        ^   ^    ^
		    |  |        |   |    |
		    |  |        |   |    `--- d (= '\0')
		    |  |        |   `-------- c
		    |  |        `------------ b (= '\0')
		    |  `--------------------- a
		    `------------------------ line
		 */
		char *a, *b, *c, *d;
		for (a = line; *a &&  isspace(*a); a++);
		for (b = a;    *b && !isspace(*b); b++);
		for (c = b;    *c &&  isspace(*c); c++);
		for (d = c;    *d && !isspace(*d); d++);
		*b = *d = '\0';

		if (!*a) continue;
		if (*a == '#') continue;
		if (*a == '\n') continue;

		kv = malloc(sizeof(cw_keyval_t));
		assert(kv);
		kv->key = strdup(a);
		kv->val = strdup(c);

		cw_list_unshift(cfg, &kv->l);
	}
	return 0;
}

int cw_cfg_write(cw_list_t *cfg, FILE *io)
{
	int rc;

	cw_list_t uniq;
	rc = cw_list_init(&uniq);
	assert(rc == 0);

	rc = cw_cfg_uniq(&uniq, cfg);
	assert(rc == 0);

	cw_keyval_t *kv;
	for_each_object(kv, &uniq, l)
		fprintf(io, "%s %s\n", kv->key, kv->val);

	return 0;
}

int cw_cfg_uniq(cw_list_t *dest, cw_list_t *src)
{
	cw_keyval_t *kv, *tmp;
	for_each_object_safe(kv, tmp, src, l) {
		if (cw_cfg_isset(dest, kv->key)) continue;
		cw_cfg_set(dest, kv->key, kv->val);
	}
	return 0;
}

int cw_cfg_done(cw_list_t *cfg)
{
	cw_keyval_t *kv, *tmp;
	for_each_object_safe(kv, tmp, cfg, l) {
		free(kv->key);
		free(kv->val);
		cw_list_delete(&kv->l);
		free(kv);
	}
	return 0;
}

/*
    ##        #######   ######    ######
    ##       ##     ## ##    ##  ##    ##
    ##       ##     ## ##        ##
    ##       ##     ## ##   ####  ######
    ##       ##     ## ##    ##        ##
    ##       ##     ## ##    ##  ##    ##
    ########  #######   ######    ######
 */

static struct {
	FILE *console;
	char *ident;
	int   level;
} CW_LOG = {
	.console = NULL,
	.ident   = NULL,
	.level   = LOG_INFO
};

void cw_log_open(const char *ident, const char *facility)
{
	assert(ident);
	assert(facility);

	free(CW_LOG.ident);
	CW_LOG.ident = strdup(ident);
	assert(CW_LOG.ident);

	if (strcmp(facility, "stdout") == 0) {
		CW_LOG.console = stdout;
		return;
	}
	if (strcmp(facility, "stderr")  == 0
	 || strcmp(facility, "console") == 0) {
		CW_LOG.console = stderr;
		return;
	}

	if (strncmp(facility, "file:", 5) == 0) {
		char *path = strchr(facility, ':'); path++;
		CW_LOG.console = fopen(path, "w+");
		return;
	}

	int fac = strcmp(facility, "local0") == 0 ? LOG_LOCAL0
	        : strcmp(facility, "local1") == 0 ? LOG_LOCAL1
	        : strcmp(facility, "local2") == 0 ? LOG_LOCAL2
	        : strcmp(facility, "local3") == 0 ? LOG_LOCAL3
	        : strcmp(facility, "local4") == 0 ? LOG_LOCAL4
	        : strcmp(facility, "local5") == 0 ? LOG_LOCAL5
	        : strcmp(facility, "local6") == 0 ? LOG_LOCAL6
	        : strcmp(facility, "local7") == 0 ? LOG_LOCAL7
	        :                                   LOG_DAEMON;

	CW_LOG.console = NULL;
	closelog();
	openlog(CW_LOG.ident, LOG_PID, fac);
}

void cw_log_close(void)
{
	if (CW_LOG.console) {
		fclose(CW_LOG.console);
		CW_LOG.console = NULL;
	} else {
		closelog();
	}

	free(CW_LOG.ident);
	CW_LOG.ident = NULL;
}

int cw_log_level(int level, const char *name)
{
	int was = CW_LOG.level;
	if (name) {
		level = cw_log_level_number(name);
		if (level < 0) level = CW_LOG.level;
	}
	if (level >= 0) {
		if (level > LOG_DEBUG)
			level = LOG_DEBUG;
		CW_LOG.level = level;
	}
	return was;
}

const char* cw_log_level_name(int level)
{
	if (level < 0) level = CW_LOG.level;
	switch (level) {
	case LOG_EMERG:   return "emergency";
	case LOG_ALERT:   return "alert";
	case LOG_CRIT:    return "critical";
	case LOG_ERR:     return "error";
	case LOG_WARNING: return "warning";
	case LOG_NOTICE:  return "notice";
	case LOG_INFO:    return "info";
	case LOG_DEBUG:   return "debug";
	default:          return "UNKNOWN";
	}
}

int cw_log_level_number(const char *name)
{
	if (!name) return -1;
	return   strcmp(name, "emerg")     == 0 ? LOG_EMERG
	       : strcmp(name, "emergency") == 0 ? LOG_EMERG
	       : strcmp(name, "alert")     == 0 ? LOG_ALERT
	       : strcmp(name, "crit")      == 0 ? LOG_CRIT
	       : strcmp(name, "critical")  == 0 ? LOG_CRIT
	       : strcmp(name, "err")       == 0 ? LOG_ERR
	       : strcmp(name, "error")     == 0 ? LOG_ERR
	       : strcmp(name, "warn")      == 0 ? LOG_WARNING
	       : strcmp(name, "warning")   == 0 ? LOG_WARNING
	       : strcmp(name, "notice")    == 0 ? LOG_NOTICE
	       : strcmp(name, "info")      == 0 ? LOG_INFO
	       : strcmp(name, "debug")     == 0 ? LOG_DEBUG
	       : -1;
}

void cw_log(int level, const char *fmt, ...)
{
	if (level > CW_LOG.level)
		return;

	va_list ap1, ap2;
	va_start(ap1, fmt);
	va_copy(ap2, ap1);
	size_t n = vsnprintf(NULL, 0, fmt, ap1);
	assert(n >= 0);
	va_end(ap1);

	char *msg = calloc(n + 1, sizeof(char));
	assert(msg);
	vsnprintf(msg, n + 1, fmt, ap2);
	msg[n] = '\0';
	va_end(ap2);

	if (CW_LOG.console) {
		assert(level >= 0 && level <= LOG_DEBUG);

		pid_t pid = getpid();
		assert(pid);

		fprintf(CW_LOG.console, "%s[%i] %s\n",
				CW_LOG.ident, pid, msg);
		fflush(CW_LOG.console);
	} else {
		syslog(level, "%s", msg);
	}
	free(msg);
}

int cw_logio(int level, const char *fmt, FILE *io)
{
	if (level > CW_LOG.level)
		return 0;

	char buf[8192];
	for (;;) {
		if (fgets(buf, sizeof(buf), io)) {
			char *nl = strchr(buf, '\n');
			if (nl) *nl = '\0';
			cw_log(level, fmt, buf);
			continue;
		}
		if (feof(io)) return 0;
		return 1;
	}
}

/*
    #######   ##     ## ##    ##
    ##    ##  ##     ## ###   ##
    ##    ##  ##     ## ####  ##
    #######   ##     ## ## ## ##
    ##   ##   ##     ## ##  ####
    ##    ##  ##     ## ##   ###
    ##     ##  #######  ##    ##
*/

# define MAXARGS 100

int cw_run2(cw_runner_t *ctx, char *cmd, ...)
{
	pid_t pid = fork();
	if (pid < 0) {
		cw_log(LOG_ERR, "Failed to fork(): %s", strerror(errno));
		return -1;
	}

	if (pid > 0) {
		int status = 0;
		waitpid(pid, &status, 0);

		if (ctx && ctx->out) rewind(ctx->out);
		if (ctx && ctx->err) rewind(ctx->err);

		if (WIFEXITED(status))
			return WEXITSTATUS(status);
		else
			return -2;
	}

	if (ctx && ctx->gid) {
		cw_log(LOG_DEBUG, "Setting GID to %u", ctx->gid);
		if (setgid(ctx->gid) != 0) {
			cw_log(LOG_ERR, "Failed to set effective GID to %u: %s",
					ctx->gid, strerror(errno));
			exit(127);
		}
	}
	if (ctx && ctx->uid) {
		cw_log(LOG_DEBUG, "Setting UID to %u", ctx->uid);
		if (setuid(ctx->uid) != 0) {
			cw_log(LOG_ERR, "Failed to set effective UID to %u: %s",
					ctx->uid, strerror(errno));
			exit(127);
		}
	}

	char *args[MAXARGS] = { 0 };
	int argno = 1;

	char *p = strrchr(cmd, '/');
	args[0] = strdup(p ? ++p : cmd);

	va_list argv;
	va_start(argv, cmd);
	while ((args[argno++] = va_arg(argv, char *)) != (char *)0)
		;

	if (ctx && ctx->in) {
		int fd = fileno(ctx->in);
		if (fd >= 0) dup2(fd, 0);
	}

	if (ctx && ctx->out) {
		int fd = fileno(ctx->out);
		if (fd >= 0) dup2(fd, 1);
	}

	if (ctx && ctx->err) {
		int fd = fileno(ctx->err);
		if (fd >= 0) dup2(fd, 2);
	}

	execvp(cmd, args);
	cw_log(LOG_DEBUG, "cw_run2: execvp('%s') failed - %s",
			cmd, strerror(errno));
	exit(127);
}

/*
    ########     ###    ######## ##     ##  #######  ##    ##
    ##     ##   ## ##   ##       ###   ### ##     ## ###   ##
    ##     ##  ##   ##  ##       #### #### ##     ## ####  ##
    ##     ## ##     ## ######   ## ### ## ##     ## ## ## ##
    ##     ## ######### ##       ##     ## ##     ## ##  ####
    ##     ## ##     ## ##       ##     ## ##     ## ##   ###
    ########  ##     ## ######## ##     ##  #######  ##    ##
 */

int cw_daemonize(const char *pidfile, const char *user, const char *group)
{
	umask(0);

	size_t n;
	int rc;
	int fd = -1;
	if (pidfile) {
		fd = open(pidfile, O_RDWR|O_CREAT, S_IRUSR|S_IWUSR);
		if (fd == -1) {
			perror(pidfile);
			exit(2);
		}
	}

	errno=0;
	struct passwd *pw = getpwnam(user);
	if (!pw) {
		fprintf(stderr, "Failed to look up user '%s': %s\n",
				user, (errno == 0 ? "user not found" : strerror(errno)));
		exit(2);
	}

	errno = 0;
	struct group  *gr = getgrnam(group);
	if (!gr) {
		fprintf(stderr, "Failed to look up group '%s': %s\n",
				group, (errno == 0 ? "group not found" : strerror(errno)));
		exit(2);
	}

	/* clean up the environment */
	const char *keepers[] = { "LANG", "SHLVL", "_", "PATH", "SHELL", "TERM" };
	rc = cw_cleanenv(6, keepers);
	assert(rc == 0);

	setenv("PWD",     "/",                           1);
	setenv("HOME",    pw->pw_dir,                    1);
	setenv("LOGNAME", pw->pw_name,                   1);
	setenv("USER",    pw->pw_name,                   1);

	/* chdir to fs root to avoid tying up mountpoints */
	rc = chdir("/");
	assert(rc == 0);

	/* child -> parent error communication pipe */
	int pfds[2];
	rc = pipe(pfds);
	assert(rc == 0);

	/* fork */
	pid_t pid = fork();
	assert(pid >= 0);

	if (pid > 0) {
		close(pfds[1]);
		char buf[8192];
		while ( (n = read(pfds[0], buf, 8192)) > 0) {
			buf[n] = '\0';
			fprintf(stderr, "%s", buf);
		}
		exit(0);
	}
	close(pfds[0]);
	char error[8192];

	if (pidfile) {
		struct flock lock;
		size_t n;

		lock.l_type   = F_WRLCK;
		lock.l_whence = SEEK_SET;
		lock.l_start  = 0;
		lock.l_len    = 0; /* whole file */

		rc = fcntl(fd, F_SETLK, &lock);
		if (rc == -1) {
			snprintf(error, 8192, "Failed to acquire lock on %s.%s\n",
					pidfile,
					(errno == EACCES || errno == EAGAIN
						? "  Is another copy running?"
						: strerror(errno)));
			n = write(pfds[1], error, strlen(error));
			if (n < 0)
				perror("failed to inform parent of our error condition");
			if (n < strlen(error))
				fprintf(stderr, "child->parent inform - only wrote %li of %li bytes\n",
					n, strlen(error));
			exit(2);
		}
	}

	/* leave session group, lose the controlling term */
	rc = (int)setsid();
	assert(rc != -1);

	if (pidfile) {
		/* write the pid file */
		char buf[8];
		snprintf(buf, 8, "%i\n", getpid());
		n = write(fd, buf, strlen(buf));
		if (n < 0)
			perror("failed to write PID to pidfile");
		if (n < strlen(buf))
			fprintf(stderr, "only wrote %li of %li bytes to pidfile\n",
				n, strlen(error));
		rc = fsync(fd);
		assert(rc == 0);

		if (getuid() == 0) {
			/* chmod the pidfile, so it can be removed */
			rc = fchown(fd, pw->pw_uid, gr->gr_gid);
			assert(rc == 0);
		}
	}

	if (getuid() == 0) {
		/* set UID/GID */
		if (gr->gr_gid != getgid()) {
			rc = setgid(gr->gr_gid);
			assert(rc == 0);
		}
		if (pw->pw_uid != getuid()) {
			rc = setuid(pw->pw_uid);
			assert(rc == 0);
		}
	}

	/* redirect standard IO streams to/from /dev/null */
	if (!freopen("/dev/null", "r", stdin))
		perror("Failed to reopen stdin </dev/null");
	if (!freopen("/dev/null", "w", stdout))
		perror("Failed to reopen stdout >/dev/null");
	if (!freopen("/dev/null", "w", stderr))
		perror("Failed to reopen stderr >/dev/null");
	close(pfds[1]);

	return 0;
}

int cw_cleanenv(int n, const char **keep)
{
	extern char **environ;
	/* clean up the environment */
	int i, j;
	for (i = 0; environ[i]; i++) {
		int skip = 0;
		for (j = 0; j < n; j++) {
			size_t len = strlen(keep[j]);
			if (strncmp(environ[i], keep[j], len) == 0
			 && environ[i][len] == '=') {
				skip = 1;
				break;
			}
		}

		if (skip)
			continue;

		char *equals = strchr(environ[i], '=');
		char *name = calloc(equals - environ[i] + 1, sizeof(char));
		memcpy(name, environ[i], equals - environ[i]);
		unsetenv(name);
		free(name);
	}
	return 0;
}

/*
    ######## ########  ##
       ##    ##     ## ##
       ##    ##     ## ##
       ##    ########  ##
       ##    ##        ##
       ##    ##        ##
       ##    ##        ########
 */

FILE* cw_tpl_erb(const char *src, cw_hash_t *facts)
{
	FILE *in  = tmpfile();
	FILE *out = tmpfile();

	if (!in || !out) {
		fclose(in);
		fclose(out);
		return NULL;
	}

	char *k, *v;
	for_each_key_value(facts, k, v)
		fprintf(in, "%s=%s\n", k, v);
	rewind(in);

	FILE *err = tmpfile();

	cw_runner_t runner = {
		.in  = in,
		.out = out,
		.err = err,
		.uid = 0,
		.gid = 0,
	};
	int rc = cw_run2(&runner, "cw-template-erb", src, NULL);
	fclose(in);

	char buf[8192];
	while (fgets(buf, 8192, err)) {
		char *p;
		if    ((p = strchr(buf, '\n')) != NULL) *p = '\0';
		while ((p = strchr(buf, '\t')) != NULL) *p = ' ';
		cw_log(LOG_ERR, "%s", buf);
	}
	fclose(err);

	if (rc == 0)
		return out;

	fclose(out);
	return NULL;
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
			cw_log(LOG_ERR, "Failed to chdir back to starting directory");
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
			cw_log(LOG_ERR, "short write: %s", strerror(errno));

		char *path = cw_alloc(namelen);
		strncpy(path, ent->fts_path+2, namelen);
		n = write(out, path, namelen);
		if (n < namelen)
			cw_log(LOG_ERR, "short write: %s", strerror(errno));
		free(path);

		if (contents && filelen >= 0) {
			n = write(out, contents, filelen);
			if (n < filelen)
				cw_log(LOG_ERR, "short write: %s", strerror(errno));
			munmap(contents, filelen);
		}
	}
	fts_close(fts);

	memset(&h, '0', sizeof(h));
	memcpy(h.magic, "BDFA", 4);
	memcpy(h.flags, "0001", 4);
	n = write(out, &h, sizeof(h));
	if (n < sizeof(h))
		cw_log(LOG_ERR, "short write: %s", strerror(errno));

	if (fchdir(cwd) != 0)
		cw_log(LOG_ERR, "Failed to chdir back to starting directory");
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
			fprintf(stderr, "Partial read error (only read %lu/%lu bytes)\n",
				n, sizeof(h));
			rc = 4;
			break;
		}

		if (memcmp(h.flags, "0001", 4) == 0)
			break;

		mode = s_c82i(h.mode);
		uid  = s_c82i(h.uid);
		gid  = s_c82i(h.gid);
		len  = s_c82i(h.namesize);

		filename = cw_alloc(len);
		n = read(in, filename, len);

		if (S_ISDIR(mode)) {
			cw_log(LOG_DEBUG, "BDFA: unpacking directory %s %06o %d:%d",
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

			cw_log(LOG_DEBUG, "BDFA: unpacking file %s %06o %d:%d (%d bytes)",
					filename, mode, uid, gid, len);
			FILE* out = fopen(filename, "w");
			if (!out) {
				perror(filename);
				continue;
			}
			if (fchmod(fileno(out), mode) != 0)
				cw_log(LOG_ERR, "chmod failed: %s", strerror(errno));
			if (fchown(fileno(out), uid, gid) != 0)
				cw_log(LOG_ERR, "chown failed: %s", strerror(errno));

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

		cw_log(LOG_DEBUG, "BDFA: setting atime/mtime to %d", ut.modtime);
		if (utime(filename, &ut) != 0) {
			perror(filename);
			rc = 1;
		}

		free(filename);
	}
	umask(umsk);
	if (fchdir(cwd) != 0)
		cw_log(LOG_ERR, "Failed to chdir back to starting directory");
	close(cwd);
	return rc;
}


/*

    ########  ########   #######   ######
    ##     ## ##     ## ##     ## ##    ##
    ##     ## ##     ## ##     ## ##
    ########  ########  ##     ## ##
    ##        ##   ##   ##     ## ##
    ##        ##    ##  ##     ## ##    ##
    ##        ##     ##  #######   ######

 */

int cw_proc_stat(pid_t pid, cw_proc_t *ps)
{
	assert(ps);      // LCOV_EXCL_LINE
	assert(pid > 0); // LCOV_EXCL_LINE

	char *path, line[8192];
	FILE *f;

	path = cw_string("/proc/%u/status", pid);
	f = fopen(path, "r");
	free(path);
	if (!f) return -1;

	while (fgets(line, 8192, f)) {
		if (sscanf(line, "Pid:\t%u\n",     &ps->pid)) continue;
		if (sscanf(line, "PPid:\t%u\n",    &ps->ppid)) continue;
		if (sscanf(line, "State:\t%c ",    &ps->state)) continue;
		if (sscanf(line, "Uid:\t%u\t%u\t", &ps->uid, &ps->euid)) continue;
		if (sscanf(line, "Gid:\t%u\t%u\t", &ps->gid, &ps->egid)) continue;
	}
	fclose(f);
	return 0;
}

/*

    ##        #######   ######  ##    ##  ######
    ##       ##     ## ##    ## ##   ##  ##    ##
    ##       ##     ## ##       ##  ##   ##
    ##       ##     ## ##       #####     ######
    ##       ##     ## ##       ##  ##         ##
    ##       ##     ## ##    ## ##   ##  ##    ##
    ########  #######   ######  ##    ##  ######

 */

static int s_cw_lock_details(cw_lock_t *lock)
{
	lock->valid = 0;

	FILE *file = fopen(lock->path, "r");
	if (!file) return 1;

	int count = fscanf(file, "LOCK p%u u%u t%i\n",
		&lock->pid, &lock->uid, &lock->time);
	fclose(file);
	if (count != 3) return 1;

	lock->valid = 1;
	return 0;
}

void cw_lock_init(cw_lock_t *lock, const char *path)
{
	assert(lock); // LCOV_EXCL_LINE
	lock->path  = path;
	lock->valid =  0;
	lock->fd    = -1;
	lock->pid   =  0;
	lock->uid   =  0;
	lock->time  = -1;
}

int cw_lock(cw_lock_t *lock, int flags)
{
	assert(lock); // LCOV_EXCL_LINE
	assert(lock->path);

	lock->valid = 0;
	lock->fd = open(lock->path, O_CREAT|O_EXCL|O_WRONLY, 0444);
	if (lock->fd < 0) {
		/* lock exists; is it still viable? */
		if (s_cw_lock_details(lock) == 0) {
			/* read lock details; check viability */

			cw_proc_t ps;
			if (cw_proc_stat(lock->pid, &ps) != 0
			&& (flags != CW_LOCK_SKIP_EUID || ps.euid != lock->uid)) {
				unlink(lock->path);
				lock->fd = open(lock->path, O_CREAT|O_EXCL|O_WRONLY, 0444);
			}
		}
	}

	if (lock->fd < 0)
		return -1;

	lock->pid  = getpid();
	lock->uid  = geteuid();
	lock->time = cw_time_s();

	char *details = cw_string("LOCK p%u u%u t%i\n", lock->pid, lock->uid, lock->time);
	size_t n = strlen(details);

	if (write(lock->fd, details, n) != n) {
		close(lock->fd);
		unlink(lock->path);
		return -1;
	}

	lock->valid = 1;
	return 0;
}

int cw_unlock(cw_lock_t *lock)
{
	assert(lock); // LCOV_EXCL_LINE
	if (lock->fd < 0) return 0;

	close(lock->fd);
	return unlink(lock->path);
}

char* cw_lock_info(cw_lock_t *lock)
{
	assert(lock); // LCOV_EXCL_LINE

	static char buf[1024];
	if (lock->valid) {
		struct passwd *pw = getpwuid(lock->uid);
		snprintf(buf, 255, "PID %u, %s(%u); locked %s",
			lock->pid, (pw ? pw->pw_name : "<unknown>"), lock->uid,
			cw_time_strf("%Y-%m-%d %H:%M:%S%z", lock->time));
	} else {
		snprintf(buf, 255, "<invalid lock file>");
	}
	return buf;
}

/*

     ######     ###     ######  ##     ## ########
    ##    ##   ## ##   ##    ## ##     ## ##
    ##        ##   ##  ##       ##     ## ##
    ##       ##     ## ##       ######### ######
    ##       ######### ##       ##     ## ##
    ##    ## ##     ## ##    ## ##     ## ##
     ######  ##     ##  ######  ##     ## ########

*/

cw_cache_t* cw_cache_new(size_t len, int32_t min_life)
{
	cw_cache_t *cc  = cw_alloc(sizeof(cw_cache_t)
	                         + sizeof(struct __cw_cce) * len);
	cc->max_len   = len;
	cc->min_life  = min_life;
	memset(&cc->index, 0, sizeof(cw_hash_t));
	return cc;
}

int cw_cache_tune(cw_cache_t **cc, size_t len, int32_t min_life)
{
	errno = EINVAL;
	if (len      <= 0) len      = (*cc)->max_len;
	if (min_life <= 0) min_life = (*cc)->min_life;

	if (len < (*cc)->max_len)
		return -1;

	if (min_life < (*cc)->min_life)
		cw_cache_purge((*cc), 0);
	(*cc)->min_life = min_life;

	if (len > (*cc)->max_len) {
		cw_cache_t *new = cw_cache_new(len, min_life);

		char *k; void *v;
		for_each_key_value(&(*cc)->index, k, v)
			cw_hash_set(&new->index, k, v);

		new->destroy_f = (*cc)->destroy_f;
		cw_cache_free((*cc));
		*cc = new;
	}

	return 0;
}

void cw_cache_free(cw_cache_t *cc)
{
	if (!cc) return;

	cw_cache_purge(cc, 1);
	cw_hash_done(&cc->index, 0);
	free(cc);
}

void cw_cache_purge(cw_cache_t *cc, int force)
{
	int32_t now = cw_time_s();
	int i;
	for (i = 0; i < cc->max_len; i++) {
		if (!force &&
		     (cc->entries[i].last_seen == -1
		   || cc->entries[i].last_seen >= now - cc->min_life))
			continue;

		if (cc->entries[i].ident) {
			cw_hash_set(&cc->index, cc->entries[i].ident, NULL);

			free(cc->entries[i].ident);
			cc->entries[i].ident = NULL;

			if (cc->destroy_f)
				(*cc->destroy_f)(cc->entries[i].data);
			cc->entries[i].data = NULL;

			cc->entries[i].last_seen = 0;
		}
	}
}

int cw_cache_opt(cw_cache_t *cc, int op, void *data)
{
	if (op == CW_CACHE_OPT_DESTROY) {
		cc->destroy_f = data;
		return 0;
	}
	if (op == CW_CACHE_OPT_MINLIFE) {
		cc->min_life = *(int*)(data) & 0xffffffff;
		return 0;
	}
	return 1;
}

static int s_cache_next(cw_cache_t *cc)
{
	int i;
	for (i = 0; i < cc->max_len; i++)
		if (!cc->entries[i].ident)
			return i;
	return -1;
}

void* cw_cache_get(cw_cache_t *cc, const char *id)
{
	struct __cw_cce *ent = cw_hash_get(&cc->index, id);
	if (!ent) return NULL;
	ent->last_seen = cw_time_s();
	return ent->data;
}

void* cw_cache_set(cw_cache_t *cc, const char *id, void *data)
{
	struct __cw_cce *ent = cw_hash_get(&cc->index, id);
	if (!ent) {
		int idx = s_cache_next(cc);
		if (idx < 0) return NULL;
		ent = &cc->entries[idx];
	}
	if (!ent->ident) {
		ent->ident = strdup(id);
		cw_hash_set(&cc->index, id, ent);
	}
	ent->last_seen = cw_time_s();
	return ent->data = data;
}

void* cw_cache_unset(cw_cache_t *cc, const char *id)
{
	struct __cw_cce *ent = cw_hash_get(&cc->index, id);
	if (!ent) return NULL;
	cw_hash_set(&cc->index, id, NULL);

	free(ent->ident);
	ent->ident = NULL;

	ent->last_seen = 0;

	void *d = ent->data;
	ent->data = NULL;

	return d;
}

/*

     ######  ##     ## ########  ##     ## ########
    ##    ## ##     ## ##     ## ##     ## ##
    ##       ##     ## ##     ## ##     ## ##
    ##       ##     ## ########  ##     ## ######
    ##       ##     ## ##   ##    ##   ##  ##
    ##    ## ##     ## ##    ##    ## ##   ##
     ######   #######  ##     ##    ###    ########

 */

cw_cert_t* cw_cert_new(void)
{
	cw_cert_t *key = cw_alloc(sizeof(cw_cert_t));
	return key;
}

cw_cert_t* cw_cert_generate(void)
{
	cw_cert_t *key = cw_cert_new();
	assert(key);

	int rc = crypto_box_keypair(key->pubkey_bin, key->seckey_bin);
	assert(rc == 0);

	rc = base16_encode(key->pubkey_b16, 64, key->pubkey_bin, 32);
	assert(rc == 64);
	key->pubkey = 1;

	base16_encode(key->seckey_b16, 64, key->seckey_bin, 32);
	assert(rc == 64);
	key->seckey = 1;

	return key;
}

cw_cert_t* cw_cert_read(const char *path)
{
	assert(path);

	FILE *io = fopen(path, "r");
	if (!io) return NULL;

	cw_cert_t *key = cw_cert_readio(io);
	fclose(io);
	return key;
}

cw_cert_t* cw_cert_readio(FILE *io)
{
	assert(io);

	/* format:
	   -----------------------------------------------
	   id  fqdn
	   pub DECAFBAD...
	   sec DEADBEEF...
	   -----------------------------------------------
	 */

	cw_list_t cfg;
	cw_list_init(&cfg);

	int rc = cw_cfg_read(&cfg, io);
	if (rc != 0) return NULL;

	cw_cert_t *key = cw_cert_new();
	assert(key);

	if (cw_cfg_isset(&cfg, "id")) {
		free(key->ident);
		key->ident = strdup(cw_cfg_get(&cfg, "id"));
	}

	char *v;
	if (cw_cfg_isset(&cfg, "pub")) {
		v = cw_cfg_get(&cfg, "pub");
		if (strlen(v) != 64) goto bail_out;

		strncpy(key->pubkey_b16, v, 64);
		key->pubkey = 1;
	}

	if (cw_cfg_isset(&cfg, "sec")) {
		v = cw_cfg_get(&cfg, "sec");
		if (strlen(v) != 64) goto bail_out;

		strncpy(key->seckey_b16, v, 64);
		key->seckey = 1;
	}

	if (!key->pubkey) goto bail_out;
	if (cw_cert_rescan(key) != 0) goto bail_out;

	cw_cfg_done(&cfg);
	return key;

bail_out:
	errno = EINVAL;
	cw_cfg_done(&cfg);
	cw_cert_destroy(key);
	return NULL;
}

int cw_cert_write(cw_cert_t *key, const char *path, int full)
{
	assert(key);
	assert(path);

	FILE *io = fopen(path, "w");
	if (!io) return -1;

	int rc = cw_cert_writeio(key, io, full);
	fclose(io);
	return rc;
}

int cw_cert_writeio(cw_cert_t *key, FILE *io, int full)
{
	assert(key);
	assert(io);

	if (key->ident)          fprintf(io, "id  %s\n", key->ident);
	if (key->pubkey)         fprintf(io, "pub %s\n", key->pubkey_b16);
	if (key->seckey && full) fprintf(io, "sec %s\n", key->seckey_b16);

	return 0;
}

void cw_cert_destroy(cw_cert_t *key)
{
	if (!key) return;
	free(key->ident);
	free(key);
}

uint8_t *cw_cert_public(cw_cert_t *key)
{
	assert(key);
	return key->pubkey_bin;
}

uint8_t *cw_cert_secret(cw_cert_t *key)
{
	assert(key);
	return key->seckey_bin;
}

char *cw_cert_public_s(cw_cert_t *key)
{
	assert(key);
	if (!key->pubkey) return NULL;
	return strdup(key->pubkey_b16);
}

char *cw_cert_secret_s(cw_cert_t *key)
{
	assert(key);
	if (!key->seckey) return NULL;
	return strdup(key->seckey_b16);
}

int cw_cert_rescan(cw_cert_t *key)
{
	assert(key);

	int rc;

	if (key->pubkey) {
		rc = base16_decode(key->pubkey_bin, 32, key->pubkey_b16, 64);
		if (rc != 32) return -1;
	}

	if (key->seckey) {
		rc = base16_decode(key->seckey_bin, 32, key->seckey_b16, 64);
		if (rc != 32) return -1;
	}

	return 0;
}

int cw_cert_encode(cw_cert_t *key)
{
	assert(key);

	int rc;

	if (key->pubkey) {
		rc = base16_encode(key->pubkey_b16, 64, key->pubkey_bin, 32);
		if (rc != 64) return -1;
	}

	if (key->seckey) {
		rc = base16_encode(key->seckey_b16, 64, key->seckey_bin, 32);
		if (rc != 64) return -1;
	}

	return 0;
}

cw_trustdb_t* cw_trustdb_new(void)
{
	cw_trustdb_t *ca = cw_alloc(sizeof(cw_trustdb_t));
	ca->verify = 1;
	cw_list_init(&ca->certs);
	return ca;
}

cw_trustdb_t* cw_trustdb_read(const char *path)
{
	assert(path);

	FILE *io = fopen(path, "r");
	if (!io) return NULL;

	cw_trustdb_t *ca = cw_trustdb_readio(io);
	fclose(io);
	return ca;
}

cw_trustdb_t* cw_trustdb_readio(FILE *io)
{
	assert(io);

	cw_trustdb_t *ca = cw_trustdb_new();
	int rc = cw_cfg_read(&ca->certs, io);
	if (rc != 0) {
		cw_trustdb_destroy(ca);
		return NULL;
	}

	return ca;
}

int cw_trustdb_write(cw_trustdb_t *ca, const char *path)
{
	assert(ca);
	assert(path);

	FILE *io = fopen(path, "w");
	if (!io) return -1;

	int rc = cw_trustdb_writeio(ca, io);
	fclose(io);
	return rc;
}

int cw_trustdb_writeio(cw_trustdb_t *ca, FILE *io)
{
	assert(ca);
	assert(io);

	cw_keyval_t *kv;
	for_each_object(kv, &ca->certs, l)
		if (kv->val)
			fprintf(io, "%s %s\n", kv->key, kv->val);

	return 0;
}

void cw_trustdb_destroy(cw_trustdb_t *ca)
{
	if (!ca) return;
	cw_cfg_done(&ca->certs);
	free(ca);
}


int cw_trustdb_trust(cw_trustdb_t *ca, cw_cert_t *key)
{
	assert(ca);
	assert(key);
	assert(key->pubkey);

	cw_cfg_set(&ca->certs, key->pubkey_b16, key->ident ? key->ident : "~");
	return 0;
}

int cw_trustdb_revoke(cw_trustdb_t *ca, cw_cert_t *key)
{
	assert(ca);
	assert(key);
	assert(key->pubkey);

	cw_cfg_unset(&ca->certs, key->pubkey_b16);
	return 0;
}

int cw_trustdb_verify(cw_trustdb_t *ca, cw_cert_t *key)
{
	assert(ca);
	assert(key);
	assert(key->pubkey);

	if (!ca->verify) return 0;

	char *ident = cw_cfg_get(&ca->certs, key->pubkey_b16);
	return ident != NULL ? 0 : 1;
}

/*

    ########    ###    ########
         ##    ## ##   ##     ##
        ##    ##   ##  ##     ##
       ##    ##     ## ########
      ##     ######### ##
     ##      ##     ## ##
    ######## ##     ## ##

 */

typedef struct {
	pthread_t     tid;
	void         *socket;
	cw_trustdb_t *tdb;
} _zap_t;

static char *s_zap_recv(void *socket)
{
	char buf[256];
	int n = zmq_recv(socket, buf, 255, 0);
	if (n < 0) return NULL;
	if (n > 255) n = 255;
	buf[n] = '\0';
	return strdup(buf);
}
static int s_zap_send(void *socket, const char *s) {
	return zmq_send(socket, s, strlen(s), 0);
}
static int s_zap_sendmore(void *socket, const char *s) {
	return zmq_send(socket, s, strlen(s), ZMQ_SNDMORE);
}
static void* s_zap_thread(void *u)
{
	assert(u);
	_zap_t *zap = (_zap_t*)u;

	cw_log(LOG_INFO, "zap: authentication thread starting up");

	for (;;) {
		cw_log(LOG_DEBUG, "zap: awaiting auth packet");
		char *version = s_zap_recv(zap->socket);
		cw_log(LOG_DEBUG, "zap: inbound auth packet!");
		if (!version) break;

		char *sequence  = s_zap_recv(zap->socket);
		char *domain    = s_zap_recv(zap->socket);
		char *address   = s_zap_recv(zap->socket);
		char *identity  = s_zap_recv(zap->socket);
		char *mechanism = s_zap_recv(zap->socket);

		cw_log(LOG_DEBUG, "zap: received frame:   version  = %s", version);
		cw_log(LOG_DEBUG, "zap: received frame:   sequence = %s", sequence);
		cw_log(LOG_DEBUG, "zap: received frame:   domain   = %s", domain);
		cw_log(LOG_DEBUG, "zap: received frame:   address  = %s", address);
		cw_log(LOG_DEBUG, "zap: received frame:   identity = %s", identity);

		cw_cert_t *key = cw_cert_new();
		assert(key);
		key->pubkey = 1;
		int n = zmq_recv(zap->socket, key->pubkey_bin, 32, 0);
		if (n != 32) goto bail_out;
		if (strcmp(version,   "1.0")   != 0) goto bail_out;
		if (strcmp(mechanism, "CURVE") != 0) goto bail_out;

		cw_log(LOG_DEBUG, "zap: verified message structure");

		cw_cert_encode(key);
		cw_log(LOG_DEBUG, "zap: checking public key [%s]", key->pubkey_b16);

		s_zap_sendmore(zap->socket, version);
		s_zap_sendmore(zap->socket, sequence);

		if (!zap->tdb || cw_trustdb_verify(zap->tdb, key) == 0) {
			cw_log(LOG_DEBUG, "zap: granting authentication request - 200 OK");
			s_zap_sendmore(zap->socket, "200");
			s_zap_sendmore(zap->socket, "OK");
			s_zap_sendmore(zap->socket, "anonymous");
			s_zap_send    (zap->socket, "");
		} else {
			cw_log(LOG_DEBUG, "zap: rejecting authentication request - 400 Untrusted");
			s_zap_sendmore(zap->socket, "400");
			s_zap_sendmore(zap->socket, "Untrusted client public key");
			s_zap_sendmore(zap->socket, "");
			s_zap_send    (zap->socket, "");
		}

		free(version);
		free(sequence);
		free(domain);
		free(address);
		free(identity);
		free(mechanism);
		cw_cert_destroy(key);

		continue;
bail_out:
		cw_log(LOG_WARNING, "zap: denying curve authentication");
		free(version);
		free(sequence);
		free(domain);
		free(address);
		free(identity);
		free(mechanism);
		cw_cert_destroy(key);
		break;
	}
	zmq_close(zap->socket);
	return zap;
}
void* cw_zap_startup(void *zctx, cw_trustdb_t *tdb)
{
	assert(zctx);

	_zap_t *handle = cw_alloc(sizeof(_zap_t));
	handle->tdb = tdb;

	handle->socket = zmq_socket(zctx, ZMQ_REP);
	assert(handle->socket);

	int rc = zmq_bind(handle->socket, "inproc://zeromq.zap.01");
	assert(rc == 0);

	rc = pthread_create(&handle->tid, NULL, s_zap_thread, handle);
	assert(rc == 0);

	return handle;
}

void cw_zap_shutdown(void *handle)
{
	if (!handle) return;

	_zap_t *z = (_zap_t*)handle;
	pthread_cancel(z->tid);

	void *_;
	pthread_join(z->tid, &_);

	cw_zmq_shutdown(z->socket, 0);
	free(z);
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
