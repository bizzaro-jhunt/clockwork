#include "string.h"

#include <ctype.h>

#define SI_COPY 0
#define SI_SREF 1 /* "simple" reference:  $([a-zA-Z][a-zA-Z0-9]*) */
#define SI_CREF 2 /* "complex" reference: ${([a-zA-Z][^}]*)} */
#define SI_ESC  3 /* a leading '\' for escaping '$' */

static char* _extract(const char *start, const char *end);
static char* _lookup(const char *ref, const struct hash *ctx);
static int   _deref(char **buf, size_t *len, const char *start, const char *end, const struct hash *ctx);

static char* _extract(const char *start, const char *end)
{
	size_t len = end - start + 1;
	char *buf = xmalloc(len * sizeof(char));

	if (!xstrncpy(buf, start, len)) {
		free(buf);
		return NULL;
	}

	return buf;
}

static char* _lookup(const char *ref, const struct hash *ctx)
{
	const char *value = hash_get(ctx, ref);
	return strdup(value ? value : "");
}

static int _deref(char **buf, size_t *len, const char *start, const char *end, const struct hash *ctx)
{
	char *ref = _extract(start, end);
	char *val = _lookup(ref, ctx);

	strncat(*buf, val, *len);
	for (; **buf; (*buf)++, (*len)--)
		;

	free(ref);
	free(val);

	return 0;
}

#define bufcopyc(b,c,l) do {\
	if ((l) > 0) { (l)--; *(b)++ = (c); } \
} while(0)

static int _extend(struct string *s, size_t n)
{
	char *tmp;
	if (n >= s->bytes) {
		n = (n / s->blk + 1) * s->blk;
		if (!(tmp = realloc(s->raw, n))) {
			return -1;
		}

		s->raw = tmp;
		/* realloc may move s->raw; reset s->p */
		s->p = s->raw + s->len;
		s->bytes = n;
	}

	return 0;
}

struct string* string_new(const char *str, size_t block)
{
	struct string *s = xmalloc(sizeof(struct string));

	s->len = 0;
	s->bytes = s->blk = (block > 0 ? block : 1024);
	s->p = s->raw = xmalloc(sizeof(char) * s->blk);

	string_append(s, str);
	return s;
}

void string_free(struct string *s)
{
	if (s) { free(s->raw); }
	free(s);
}

int string_append(struct string *s, const char *str)
{
	if (!str) { return 0; }
	if (_extend(s, s->len + strlen(str)) != 0) { return -1; }

	for (; *str; *s->p++ = *str++, s->len++)
		;
	*s->p = '\0';
	return 0;
}

int string_append1(struct string *s, char c)
{
	if (_extend(s, s->len+1) != 0) { return -1; }

	*s->p++ = c;
	s->len++;
	*s->p = '\0';
	return 0;
}

int string_interpolate(char *buf, size_t len, const char *src, const struct hash *ctx)
{
	assert(buf);
	assert(src);
	assert(ctx);

	int state = SI_COPY;
	const char *ref;

	memset(buf, 0, len);
	//char *debug = buf;

	while (*src && len > 0) {
		if (state == SI_SREF) {
			if (!isalnum(*src)) {
				_deref(&buf, &len, ref, src, ctx);
				state = SI_COPY;
			}
		} else if (state == SI_CREF) {
			if (*src == '}') {
				_deref(&buf, &len, ref, src, ctx);
				src++;
				state = SI_COPY;
			}
		}

		if (state == SI_ESC) {
			bufcopyc(buf, *src++, len);
			state = SI_COPY;
			continue;
		}

		if (state == SI_COPY) {
			if (*src == '\\') {
				state = SI_ESC;
				src++;
				continue;

			} else if (*src == '$') {
				state = SI_SREF;
				src++;

				if (*src && *src == '{') {
					state = SI_CREF;
					src++;
				}

				ref = src;
				continue;
			}

			bufcopyc(buf, *src, len);
		}

		src++;
	}

	if (state != SI_COPY) {
		_deref(&buf, &len, ref, src, ctx);
	}
	*buf = '\0';

	return 0;
}
