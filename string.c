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

/**
  Create a new variable-length string

  If $str is not NULL, the new string will contain a copy of
  its contents.

  $block can be used to influence the memory management of the
  string.  When more memory is needed, it will be allocated in
  multiples of $block.  
  If $block is negative, a suitable default block size will
  be used.

  **Note:** The pointer returned by this function must be passed to
  @string_free in order to reclaim the memory it uses.

  On success, returns a pointer to a dynamically-allocated
  variable-length string structure.
  On failure, returns NULL.
 */
struct string* string_new(const char *str, size_t block)
{
	struct string *s = xmalloc(sizeof(struct string));

	s->len = 0;
	s->bytes = s->blk = (block > 0 ? block : 1024);
	s->p = s->raw = xmalloc(sizeof(char) * s->blk);

	string_append(s, str);
	return s;
}

/**
  Free a variable-length string
 */
void string_free(struct string *s)
{
	if (s) { free(s->raw); }
	free(s);
}

/**
  Append a C-string to the end of $s

  The arguments are ordered according to the following mnemonic:

  <code>
  string_add(my_string, " and then some")
  // my_string += " and then some"
  </code>

  $str must be a NULL-terminated C-style character string.

  On success, returns 0.  On failure, returns non-zero and
  $s is left unmodified.
 */
int string_append(struct string *s, const char *str)
{
	if (!str) { return 0; }
	if (_extend(s, s->len + strlen(str)) != 0) { return -1; }

	for (; *str; *s->p++ = *str++, s->len++)
		;
	*s->p = '\0';
	return 0;
}

/**
  Append a single character to $s

  The arguments are ordered according to the following mnemonic:

  <code>
  string_add(my_string, '!');
  // my_string += '!'
  </code>

  On success, returns 0.  On failure, returns non-zero and
  $s is left unmodified.
 */
int string_append1(struct string *s, char c)
{
	if (_extend(s, s->len+1) != 0) { return -1; }

	*s->p++ = c;
	s->len++;
	*s->p = '\0';
	return 0;
}

/**
  Interpolate variable references in $src against $ctx

  Variable interpolation is performed according to the following rules:

  `$([a-zA-Z][a-zA-Z0-9_-]*)` is expanded to the string value stored in the 
  $ctx hash under the key \1.  For example, in the string "My name is \$name",
  "\$name" is taken as a reference to the value stored in $ctx under the key
  "name".

  Complex key names are also supported, using the format `${([^}]+)}`.  To give
  another example, the string "My name is ${person.name}" contains a reference
  to the value stored in $ctx under the key "person.name".

  The caller must take care to make $buf large enough to accommodate the
  expanded string.  If the buffer is not long enough, this function will only
  fill $buf with the first $len bytes of the interpolated result.

  The resultant NULL-terminated string (with all variables expanded) will be
  stored in $buf.  The length of $buf (not including the NULL-terminator) will
  be placed in $len.

  On success, returns 0.  On failure, returns non-zero.
 */
int string_interpolate(char *buf, size_t len, const char *src, const struct hash *ctx)
{
	assert(buf); // LCOV_EXCL_LINE
	assert(src); // LCOV_EXCL_LINE
	assert(ctx); // LCOV_EXCL_LINE

	int state = SI_COPY;
	const char *ref;

	memset(buf, 0, len);
	//char *debug = buf;

	--len; // we really only have len-1 character slots (trailing \0)
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
