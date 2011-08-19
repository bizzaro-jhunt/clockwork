#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "pack.h"
#include "mem.h"

static char* _escape(const char *string)
{
	char *escaped, *write_ptr;
	const char *read_ptr;
	size_t len;

	if (!string) {
		return strdup("");
	}

	for (len = 0, read_ptr = string; *read_ptr; read_ptr++) {
		len++;
		if (*read_ptr == '"') {
			len++;
		}
	}

	escaped = xmalloc(len + 1);
	for (write_ptr = escaped, read_ptr = string; *read_ptr; read_ptr++) {
		if (*read_ptr == '"') {
			*write_ptr++ = '\\';
		}
		*write_ptr++ = *read_ptr;
	}
	*write_ptr = '\0';

	return escaped;
}

static char* _unescape(const char *string)
{
	char *unescaped, *write_ptr;
	const char *read_ptr;
	size_t len;
	char last = '\0';

	for (len = 0, read_ptr = string; *read_ptr; read_ptr++) {
		len++;
		if (*read_ptr == '"' && last == '\\') {
			len--;
		}
		last = *read_ptr;
	}

	last = '\0';
	unescaped = xmalloc(len + 1);
	for (write_ptr = unescaped, read_ptr = string; *read_ptr; read_ptr++) {
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

static size_t _packlen(const char *prefix, const char *format, va_list args)
{
	size_t len = strlen(prefix);
	char *escaped;

	while (*format) {
		switch (*format++) {
		case 'a': /* NULL-terminated character string */
			escaped = _escape(va_arg(args, const char*));
			len += strlen(escaped) + 2; /* '"STRING"' */
			free(escaped);
			break;

		case 'c': /* signed char   (8-bit) */
		case 'C': /* unsigned char (8-bit) */
			va_arg(args, unsigned long);
			len += 2; /* '5f' */
			break;

		case 's': /* signed short   (16-bit) */
		case 'S': /* unsigned short (16-bit) */
			va_arg(args, unsigned long);
			len += 4; /* 'cd4f' */
			break;

		case 'l': /* signed long   (32-bit) */
		case 'L': /* unsigned long (32-bit) */
			va_arg(args, unsigned long);
			len += 8; /* '00cc18f3' */
			break;
		}
	}

	return len + 1; /* trailing '\0'; */
}


/************************************************************************/

char* pack(const char *prefix, const char *format, ...)
{
	assert(prefix); // LCOV_EXCL_LINE
	assert(format); // LCOV_EXCL_LINE

	char *dst, *p;
	size_t n, len = 0;
	va_list args;

	union {
		char    *string;
		unsigned char  u8;
		unsigned short u16;
		unsigned long  u32;
	} variadic;

	va_start(args, format);
	len = _packlen(prefix, format, args);
	va_end(args);

	p = dst = xmalloc(len * sizeof(char));

	n = strlen(prefix); len -= n;
	if (len >= 0) {
		p += snprintf(p, n+1, "%s", prefix);
	}

	va_start(args, format);
	while (*format) {
		switch(*format++) {
		case 'a': /* NULL-terminated character string */
			variadic.string = _escape(va_arg(args, const char *));
			n = strlen(variadic.string) + 2; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "\"%s\"", variadic.string);
			}

			free(variadic.string);
			break;

		case 'c': /* signed char (8-bit) */
			variadic.u8 = (unsigned char)va_arg(args, signed int);
			n = 2; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%02x", variadic.u8);
			}
			break;

		case 'C': /* unsigned char (8-bit) */
			variadic.u8 = (unsigned char)va_arg(args, unsigned int);
			n = 2; len -= n;
			if (len >= 0) {
				p += snprintf(p, n+1, "%02x", variadic.u8);
			}
			break;

		case 's': /* signed short (16-bit) */
			variadic.u16 = (unsigned short)va_arg(args, signed int);
			n = 4; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%04x", variadic.u16);
			}
			break;

		case 'S': /* unsigned short (16-bit) */
			variadic.u16 = (unsigned short)va_arg(args, unsigned int);
			n = 4; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%04x", variadic.u16);
			}
			break;

		case 'l': /* signed long (32-bit) */
			variadic.u32 = va_arg(args, signed long);
			n = 8; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%08lx", variadic.u32);
			}
			break;

		case 'L': /* unsigned long (32-bit) */
			variadic.u32 = va_arg(args, unsigned long);
			n = 8; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%08lx", variadic.u32);
			}
			break;
		}
	}
	va_end(args);

	if (--len >= 0) { *p = '\0'; }

	return dst;
}

static char* _extract_string(const char *start)
{
	char last, *buf;
	const char *a, *b;

	for (a = start; *a && *a++ != '"'; )
		;
	for (last = '\0', b = a; *b && !(last != '\\' && *b == '"'); last = *b, b++)
		;

	buf = xmalloc(b - a + 1);
	memcpy(buf, a, b - a);
	buf[b-a] = '\0';

	return buf;
}

int unpack(const char *packed, const char *prefix, const char *format, ...)
{
	assert(format); // LCOV_EXCL_LINE

	char hex[9]; /* MAX size of integer hex string */
	va_list args;

	/* for string extraction */
	char *escaped, *unescaped;
	size_t l;

	l = strlen(prefix);
	if (strncmp(packed, prefix, l) != 0) { return -1; }
	packed += l;

	va_start(args, format);
	while (*format) {
		switch (*format++) {
		case 'a': /* NULL-terminated character string */
			escaped = _extract_string(packed);
			packed += strlen(escaped) + 2; /* +2 for quotes */

			unescaped = _unescape(escaped);
			free(escaped);

			*(va_arg(args, char **)) = unescaped;
			break;
		case 'c': /* signed char    (8-bit) */
			memcpy(hex, packed, 2);
			hex[2] = '\0';
			packed += 2;
			*(va_arg(args, signed char *)) = strtol(hex, NULL, 16);
			break;
		case 'C': /* unsigned char  (8-bit) */
			memcpy(hex, packed, 2);
			hex[2] = '\0';
			packed += 2;
			*(va_arg(args, unsigned char *)) = strtoul(hex, NULL, 16);
			break;
		case 's': /* signed short   (16-bit) */
			memcpy(hex, packed, 4);
			hex[4] = '\0';
			packed += 4;
			*(va_arg(args, signed short *)) = strtol(hex, NULL, 16);
			break;
		case 'S': /* unsigned short (16-bit) */
			memcpy(hex, packed, 4);
			hex[4] = '\0';
			packed += 4;
			*(va_arg(args, unsigned short *)) = strtoul(hex, NULL, 16);
			break;
		case 'l': /* signed long    (32-bit) */
			memcpy(hex, packed, 8);
			hex[8] = '\0';
			packed += 8;
			*(va_arg(args, signed long *)) = strtoll(hex, NULL, 16);
			break;
		case 'L': /* unsigned long  (32-bit) */
			memcpy(hex, packed, 8);
			hex[8] = '\0';
			packed += 8;
			*(va_arg(args, unsigned long *)) = strtoull(hex, NULL, 16);
			break;
		}
	}
	va_end(args);

	return 0;
}

