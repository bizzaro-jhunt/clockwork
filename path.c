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

#include "clockwork.h"
#include "path.h"

struct path* path_new(const char *s)
{
	struct path *p;
	if (!s) { return NULL; }

	p = xmalloc(sizeof(struct path));
	p->n = p->len = strlen(s);
	p->buf = xmalloc((p->len+2) * sizeof(char));
	memcpy(p->buf, s, p->len);
	return p;
}

void path_free(struct path *p)
{
	if (p) {
		free(p->buf);
	}
	free(p);
}

const char *path(struct path *p)
{
	return p->buf;
}

int path_canon(struct path *p)
{
	char *s, *a, *b, *end;
	if (p->len == 0) { return 0; }

	s = p->buf;
	end = s + p->len;
	for (b = s+1; b <= end; b++) {
		if (b == end || *b == '/') {
			for (a = b-1; a != s && *a != '/'; a--)
				;
			if (b-a == 3 && memcmp(a, "/..", b-a) == 0) {
				for (a--; a >= s && *a != '/'; a--)
						;
				if (a <= s) a = s; // avoid underflow
				memset(a, 0, b-a);
			} else if (b-a == 2 && memcmp(a, "/.", b-a) == 0) {
				memset(a, 0, b-a);
			}
		}
	}

	for (a = b = s; a != end; a++) {
		if (*a) { *b++ = *a; }
	}
	if (b == s) {
		*b++ = '/';
	} else if (*(b-1) == '/') {
		*b-- = '\0';
	}
	*b = '\0';

	p->n = p->len = strlen(s);
	if (p->len == 0) {
		*s = '/';
		p->n = p->len = 1;
	}
	return 0;
}

int path_push(struct path *p)
{
	char *s = p->buf;
	while (*(++s))
		;
	if (*(s+1)) { *s = '/'; }
	p->n = strlen(s);
	return *s;
}

int path_pop(struct path *p)
{
	char *s;
	for (s = p->buf + p->n; s > p->buf; s--, p->n--) {
		if (*s == '/') {
			*s = '\0';
			return 1;
		}
	}
	return 0;
}

