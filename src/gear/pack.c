/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

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

#include <assert.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "gear.h"

static int isescaped(char c)
{
	return (c == '"' || c == '\\');
}

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
		if (isescaped(*read_ptr))
			len++;
	}

	escaped = calloc(len + 1, sizeof(char));
	if (!escaped) { return NULL; }
	for (write_ptr = escaped, read_ptr = string; *read_ptr; read_ptr++) {
		if (isescaped(*read_ptr))
			*write_ptr++ = '\\';
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
		if (isescaped(*read_ptr) && last == '\\')
			len--;
		last = *read_ptr;
	}

	last = '\0';
	unescaped = calloc(len + 1, sizeof(char));
	if (!unescaped) { return NULL; }
	for (write_ptr = unescaped, read_ptr = string; *read_ptr; read_ptr++) {
		if ((isescaped(*read_ptr) && last == '\\') || *read_ptr != '\\')
			*write_ptr++ = *read_ptr;
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
			va_arg(args, uint32_t); /* throw away value */
			len += 2; /* '5f' */
			break;

		case 's': /* signed short   (16-bit) */
		case 'S': /* unsigned short (16-bit) */
			va_arg(args, uint32_t); /* throw away value */
			len += 4; /* 'cd4f' */
			break;

		case 'l': /* signed long   (32-bit) */
		case 'L': /* unsigned long (32-bit) */
			va_arg(args, uint32_t); /* throw away value */
			len += 8; /* '00cc18f3' */
			break;
		}
	}

	return len + 1; /* trailing '\0'; */
}


/************************************************************************/

/**
  Pack values into a string.

  This function serialized complex objects and structures into simple
  ASCII strings, long-term storage, network transmission, and more.  It
  works on the same principles as Perl's pack operator: the caller supplies
  a format string and a avariable list of values.

  The $format string consists of one or more single character field
  specifiers, each indicating what type of data is to be packed, and
  controlling how it gets packed.

  Possible format field specifiers are:

  - `a` - NULL-terminated character string (`const char*`)
  - `c` - Signed 8-bit integer (`char`)
  - `C` - Unsigned 8-bit integer (`unsigned char`)
  - `s` - Signed 16-bit integer (`short`)
  - `S` - Unsigned 16-bit integer (`unsigned short`)
  - `l` - Signed 32-bit integer (`long`)
  - `L` - Unsigned 32-bit integer (`unsigned long`)

  For the integer fields, c = char, s = short and l = long.
  The upper case field specifiers are for unsigned data types,
  while the lower case field sepcifiers are for signed data
  types.

  All integer fields (cCsSlL) are represented in hexadecimal,
  without the leading '0x':

  <code>
  buf = pack("", "c", 255);   // buf == "ff"
  buf = pack("", "s", 48879); // buf == "beef"
  </code>

  Signed integers are represented in hexadecimal as two's complements:

  <code>
  buf = pack("C", -42);   // buf == "d6" (11010110b / 214d)
  </code>

  The packed string will be dynamically allocated to be just large enough.
  It is left to the caller to free it when it is no longer needed.

  If given, $prefix will be prepended to the packed string before it
  is returned.

  On success, returns a packed string.  On failure, returns NULL.
 */
char* pack(const char *prefix, const char *format, ...)
{
	assert(prefix); // LCOV_EXCL_LINE
	assert(format); // LCOV_EXCL_LINE

	char *dst, *p;
	size_t n, len = 0;
	va_list args;

	union {
		char    *string;
		uint8_t   u8;
		uint16_t  u16;
		uint32_t  u32;
	} variadic;

	va_start(args, format);
	len = _packlen(prefix, format, args);
	va_end(args);

	p = dst = calloc(len, sizeof(char));
	if (!p) { return NULL; }

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
			variadic.u8 = (int8_t)va_arg(args, signed int);
			n = 2; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%02x", variadic.u8);
			}
			break;

		case 'C': /* unsigned char (8-bit) */
			variadic.u8 = (uint8_t)va_arg(args, unsigned int);
			n = 2; len -= n;
			if (len >= 0) {
				p += snprintf(p, n+1, "%02x", variadic.u8);
			}
			break;

		case 's': /* signed short (16-bit) */
			variadic.u16 = (int16_t)va_arg(args, signed int);
			n = 4; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%04x", (signed int)variadic.u16);
			}
			break;

		case 'S': /* unsigned short (16-bit) */
			variadic.u16 = (uint16_t)va_arg(args, unsigned int);
			n = 4; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%04x", (unsigned int)variadic.u16);
			}
			break;

		case 'l': /* signed long (32-bit) */
			variadic.u32 = (int32_t)va_arg(args, signed long);
			n = 8; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%08lx", (signed long)variadic.u32);
			}
			break;

		case 'L': /* unsigned long (32-bit) */
			variadic.u32 = (uint32_t)va_arg(args, unsigned long);
			n = 8; len -= n;
			if (len > 0) {
				p += snprintf(p, n+1, "%08lx", (unsigned long)variadic.u32);
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
	for (last = '\0', b = a; *b && (last == '\\' || *b != '"'); last = *b++)
		;

	buf = calloc(b - a + 1, sizeof(char));
	if (!buf) { return NULL; }
	memcpy(buf, a, b - a);
	buf[b-a] = '\0';

	return buf;
}

/**
  Unpack $packed into a set of variables.

  For the details of $format, see @pack.

  If $prefix is specified, this function will require that $packed
  begins with $prefix, or fail.

  ON success, returns 0.  On failure, returns non-zero.
 */
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
			*(va_arg(args, int8_t *)) = strtol(hex, NULL, 16);
			break;
		case 'C': /* unsigned char  (8-bit) */
			memcpy(hex, packed, 2);
			hex[2] = '\0';
			packed += 2;
			*(va_arg(args, uint8_t *)) = strtoul(hex, NULL, 16);
			break;
		case 's': /* signed short   (16-bit) */
			memcpy(hex, packed, 4);
			hex[4] = '\0';
			packed += 4;
			*(va_arg(args, int16_t *)) = strtol(hex, NULL, 16);
			break;
		case 'S': /* unsigned short (16-bit) */
			memcpy(hex, packed, 4);
			hex[4] = '\0';
			packed += 4;
			*(va_arg(args, uint16_t *)) = strtoul(hex, NULL, 16);
			break;
		case 'l': /* signed long    (32-bit) */
			memcpy(hex, packed, 8);
			hex[8] = '\0';
			packed += 8;
			*(va_arg(args, int32_t *)) = strtoll(hex, NULL, 16);
			break;
		case 'L': /* unsigned long  (32-bit) */
			memcpy(hex, packed, 8);
			hex[8] = '\0';
			packed += 8;
			*(va_arg(args, uint32_t *)) = strtoull(hex, NULL, 16);
			break;
		}
	}
	va_end(args);

	return 0;
}

