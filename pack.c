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


/************************************************************************/

size_t pack(char *dst, size_t len, const char *format, ...)
{
	/* dst can be NULL, if we are trying to find
	   the minimum size for the buffer */
	assert(format);

	size_t nbytes = 0;
	va_list args;

	union {
		char    *string;
		unsigned char  u8;
		unsigned short u16;
		unsigned long  u32;
	} variadic;

	va_start(args, format);
	while (*format) {
		switch(*format++) {
		case 'a': /* NULL-terminated character string */
			variadic.string = _escape(va_arg(args, const char *));
			nbytes += strlen(variadic.string) + 2;
			len -= strlen(variadic.string) + 2;
			if (len > 0 && dst) {
				dst += snprintf(dst, strlen(variadic.string) + 2 + 1, "\"%s\"", variadic.string);
			}

			free(variadic.string);
			break;
		case 'c': /* signed char    (8-bit) */
			variadic.u8 = (unsigned char)va_arg(args, signed int);
			nbytes += 2; len -= 2;
			if (len > 0 && dst) {
				dst += snprintf(dst, 3, "%02x", variadic.u8);
			}
			break;
		case 'C': /* unsigned char  (8-bit) */
			variadic.u8 = (unsigned char)va_arg(args, unsigned int);
			nbytes += 2; len -= 2;
			if (len >= 0 && dst) {
				dst += snprintf(dst, 3, "%02x", variadic.u8);
			}
			break;
		case 's': /* signed short   (16-bit) */
			variadic.u16 = (unsigned short)va_arg(args, signed int);
			nbytes += 4; len -= 4;
			if (len >= 0 && dst) {
				dst += snprintf(dst, 5, "%04x", variadic.u16);
			}
			break;
		case 'S': /* unsigned short (16-bit) */
			variadic.u16 = (unsigned short)va_arg(args, unsigned int);
			nbytes += 4; len -= 4;
			if (len >= 0 && dst) {
				dst += snprintf(dst, 5, "%04x", variadic.u16);
			}
			break;
		case 'l': /* signed long    (32-bit) */
			variadic.u32 = va_arg(args, signed long);
			nbytes += 8; len -= 8;
			if (len >= 0 && dst) {
				dst += snprintf(dst, 9, "%08lx", variadic.u32);
			}
			break;
		case 'L': /* unsigned long  (32-bit) */
			variadic.u32 = va_arg(args, unsigned long);
			nbytes += 8; len -= 8;
			if (len >= 0 && dst) {
				dst += snprintf(dst, 9, "%08lx", variadic.u32);
			}
			break;
		}
	}
	va_end(args);

	nbytes += 1; len -= 1;
	if (len >= 0 && dst) {
		*dst = '\0';
	}

	return nbytes;
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

int unpack(const char *packed, const char *format, ...)
{
	assert(format);

	char hex[9]; /* MAX size of integer hex string */
	va_list args;

	/* for string extraction */
	char *escaped, *unescaped;

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

