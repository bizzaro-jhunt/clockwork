#include "clockwork.h"
#include "path.h"

struct cw_path {
	char *buf;
	ssize_t n;
	size_t len;
};

PATH* path_new(const char *s)
{
	PATH *p;
	if (!s) { return NULL; }

	p = xmalloc(sizeof(PATH));
	p->n = p->len = strlen(s);
	p->buf = xmalloc((p->len+2) * sizeof(char));
	memcpy(p->buf, s, p->len);
	return p;
}

void path_free(PATH *p)
{
	if (p) {
		free(p->buf);
	}
	free(p);
}

const char *path(PATH *p)
{
	return p->buf;
}

int path_canon(PATH *p)
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
				memset(a, 0, b-a);
			} else if (b-a == 2 && memcmp(a, "/.", b-a) == 0) {
				memset(a, 0, b-a);
			}
		}
	}

	for (a = b = s; a != end; a++) {
		if (*a) { *b++ = *a; }
	}
	*b = '\0';
	/* remove trailing '/' if present */
	if (*(--b) == '/' && b != s) { *b = '\0'; }

	p->n = p->len = strlen(s);
	if (p->len == 0) {
		*s = '/';
		p->n = p->len = 1;
	}
	return 0;
}

int path_push(PATH *p)
{
	char *s = p->buf;
	while (*(++s))
		;
	if (*(s+1)) { *s = '/'; }
	p->n = strlen(s);
	return *s;
}

int path_pop(PATH *p)
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

