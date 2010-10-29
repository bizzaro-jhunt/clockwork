#include <assert.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "serialize.h"

#define SERIALIZER_BUF_MAX 8192

struct _serializer {
	char buf[SERIALIZER_BUF_MAX];
	char *cursor;

	size_t len;
};

struct _unserializer {
	char  *data;   /* Serialized string */
	char  *cursor;
	char  *end;    /* Pointer to last character in data
	                  not counting the null terminator. */

	size_t len;    /* Length of data string */
};

#define serializer_buffer_left(s) (SERIALIZER_BUF_MAX - (s)->len)
#define serializer_buffer_full(s) (serializer_buffer_left(s) == 0)

serializer* serializer_new() {
	serializer *s;

	s = malloc(sizeof(serializer));
	if (!s) {
		return NULL;
	}

	s->len = 0;
	memset(s->buf, '\0', SERIALIZER_BUF_MAX);
	s->cursor = s->buf;
}

void serializer_free(serializer *s)
{
	free(s);
}

int serializer_data(const serializer *s, char **dst, size_t *len)
{
	assert(s);
	assert(dst);
	assert(len);

	*len = s->len;
	*dst = strndup(s->buf, s->len);
	return 0;
}

char* serializer_escape(const char *data)
{
	char *escaped, *write_ptr;
	const char *read_ptr;
	size_t len;

	for (len = 0, read_ptr = data; *read_ptr; read_ptr++) {
		len++;
		if (*read_ptr == '"') {
			len++;
		}
	}

	escaped = malloc(len + 1);
	for (write_ptr = escaped, read_ptr = data; *read_ptr; read_ptr++) {
		if (*read_ptr == '"') {
			*write_ptr++ = '\\';
		}
		*write_ptr++ = *read_ptr;
	}
	*write_ptr = '\0';

	return escaped;
}

int serializer_start(serializer *s)
{
	if (serializer_buffer_full(s)) {
		return -1;
	}
	*(s->cursor++) = '{';
	s->len++;

	return 0;
}

int serializer_finish(serializer *s)
{
	if (serializer_buffer_full(s)) {
		return -1;
	}

	*(s->cursor++) = '}';
	s->len++;

	return 0;
}

#define serializer_add_macro(s,fmt,d) do { \
	int nwritten; \
	size_t space = serializer_buffer_left(s); \
	if ((s)->len == 1) { \
		nwritten = snprintf((s)->cursor, space,  "\"" fmt "\"", (d)); \
	} else { \
		nwritten = snprintf((s)->cursor, space, ":\"" fmt "\"", (d)); \
	} \
	if (nwritten < 0) { \
		return -1; \
	} else if (nwritten > space) { \
		return -2; \
	} \
	s->len += nwritten; \
	s->cursor += nwritten; \
} while (0);

int serializer_add_string(serializer *s, const char *data)
{
	assert(s);

	char *escaped = NULL;
	if (data) {
		escaped = serializer_escape(data);
		/* FIXME: check escaped == NULL */
		serializer_add_macro(s, "%s", escaped);
		free(escaped);
	} else {
		serializer_add_macro(s, "%s", "");
	}
	return 0;
}

int serializer_add_character(serializer *s, unsigned char data)
{
	assert(s);
	serializer_add_macro(s, "%c", data);
	return 0;
}

int serializer_add_uint8(serializer *s, uint8_t data)
{
	assert(s);
	serializer_add_macro(s, "%u", data);
	return 0;
}

int serializer_add_uint16(serializer *s, uint16_t data)
{
	assert(s);
	serializer_add_macro(s, "%u", data);
	return 0;
}

int serializer_add_uint32(serializer *s, uint32_t data)
{
	assert(s);
	serializer_add_macro(s, "%u", data);
	return 0;
}

int serializer_add_int8(serializer *s, int8_t data)
{
	assert(s);
	serializer_add_macro(s, "%i", data);
	return 0;
}

int serializer_add_int16(serializer *s, int16_t data)
{
	assert(s);
	serializer_add_macro(s, "%i", data);
	return 0;
}

int serializer_add_int32(serializer *s, int32_t data)
{
	assert(s);
	serializer_add_macro(s, "%i", data);
	return 0;
}

unserializer* unserializer_new(const char *data, size_t len) {
	assert(data);

	unserializer *u;

	u = malloc(sizeof(unserializer));
	if (!u) {
		return NULL;
	}

	u->len = len;
	u->data = strndup(data, len + 1);
	u->cursor = u->data;
	u->end = u->data + len;

	return u;
}

void unserializer_free(unserializer *u)
{
	if (!u) {
		return;
	}

	free(u->data);
	free(u);
}

char* unserializer_unescape(const char *data)
{
	char *unescaped, *write_ptr;
	const char *read_ptr;
	size_t len;
	char last = '\0';

	for (len = 0, read_ptr = data; *read_ptr; read_ptr++) {
		len++;
		if (*read_ptr == '"' && last == '\\') {
			len--;
		}
		last = *read_ptr;
	}

	last = '\0';
	unescaped = malloc(len + 1);
	for (write_ptr = unescaped, read_ptr = data; *read_ptr; read_ptr++) {
		if (last == '\\') {
			if (*read_ptr == '"') {
				*write_ptr++ = '"';
			} else {
				*write_ptr++ = '\\';
				*write_ptr++ = *read_ptr;
			}
		} else if (*read_ptr != '\\') {
			*write_ptr++ = *read_ptr;
		}
		last = *read_ptr;
	}
	*write_ptr = '\0';

	return unescaped;
}

int unserializer_start(unserializer *u)
{
	assert(u);

	char *p;

	for (p = u->data; p != u->end && *p != '{'; p++)
		;

	if (p == u->end) {
		return -1;
	}

	u->cursor = p;
	return 0;
}

int unserializer_next_string(unserializer *u, char **dst, size_t *len)
{
	assert(u);
	assert(dst);
	assert(len);

	char *escaped;
	char last = '\0';
	char *a, *b;
	for (a = u->cursor; a != u->end && *a++ != '"';)
		;

	if (a == u->end) {
		return -1;
	}

	for (b = a; b != u->end; b++) {
		if (last != '\\' && *b == '"') {
			break;
		}
		last = *b;
	}

	escaped = malloc(b - a + 1);
	if (!escaped) {
		return -2;
	}
	memcpy(escaped, a, b - a);
	escaped[b - a] = '\0';

	*dst = unserializer_unescape(escaped);
	free(escaped);
	*len = strlen(*dst);

	u->cursor = ++b;
	return 0;
}

int unserializer_next_character(unserializer *u, char *data)
{
	assert(u);
	assert(data);

	char *raw;
	size_t len;

	if (unserializer_next_string(u, &raw, &len) != 0) {
		return -1;
	}

	*data = raw[0];
	free(raw);

	return 0;
}

int unserializer_next_uint8(unserializer *u, uint8_t *data)
{
	assert(u);
	assert(data);

	char *raw;
	size_t len;

	if (unserializer_next_string(u, &raw, &len) != 0) {
		return -1;
	}

	if (len <= 0) {
		*data = 0;
		return 0;
	}

	*data = strtoul(raw, NULL, 10);
	return 0;
}

int unserializer_next_uint16(unserializer *u, uint16_t *data)
{
	assert(u);
	assert(data);

	char *raw;
	size_t len;

	if (unserializer_next_string(u, &raw, &len) != 0) {
		return -1;
	}

	if (len <= 0) {
		*data = 0;
		return 0;
	}

	*data = strtoul(raw, NULL, 10);
	return 0;
}

int unserializer_next_uint32(unserializer *u, uint32_t *data)
{
	assert(u);
	assert(data);

	char *raw;
	size_t len;

	if (unserializer_next_string(u, &raw, &len) != 0) {
		return -1;
	}

	if (len <= 0) {
		*data = 0;
		return 0;
	}

	*data = strtoul(raw, NULL, 10);
	return 0;
}

int unserializer_next_int8(unserializer *u, int8_t *data)
{
	assert(u);
	assert(data);

	char *raw;
	size_t len;

	if (unserializer_next_string(u, &raw, &len) != 0) {
		return -1;
	}

	if (len <= 0) {
		*data = 0;
		return 0;
	}

	*data = strtoll(raw, NULL, 10);
	return 0;
}

int unserializer_next_int16(unserializer *u, int16_t *data)
{
	assert(u);
	assert(data);

	char *raw;
	size_t len;

	if (unserializer_next_string(u, &raw, &len) != 0) {
		return -1;
	}

	if (len <= 0) {
		*data = 0;
		return 0;
	}

	*data = strtoll(raw, NULL, 10);
	return 0;
}

int unserializer_next_int32(unserializer *u, int32_t *data)
{
	assert(u);
	assert(data);

	char *raw;
	size_t len;

	if (unserializer_next_string(u, &raw, &len) != 0) {
		return -1;
	}

	if (len <= 0) {
		*data = 0;
		return 0;
	}

	*data = strtoll(raw, NULL, 10);
	return 0;
}

