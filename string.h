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

#ifndef STRING_H
#define STRING_H

#include "clockwork.h"
#include "hash.h"

/** @file string.h
 */

/**
  Variable-length Character String
 */
struct string {
	/** Length of the current string in characters. */
	size_t len;
	/** Length of the string buffer in bytes. */
	size_t bytes;
	/** String "block size"; minimum size of string buffer expansion. */
	size_t blk;

	/** NULL-terminated character string. */
	char *raw;
	/** Internal pointer to the NULL-terminator of \a raw. */
	char *p;
};

/**
  Allocate and initialize a new variable-length string

  If \a str is passed as a valid pointer (i.e. not NULL), then
  the new variable-length string will contain a copy of the
  contents of \a str.

  If \a block is negative, a suitable default block size will
  be used.

  @note The pointer returned by this function must be passed to
        string_free in order to reclaim the memory it uses.

  @param  str    (Optional) initial string value.
  @param  block  Block size for string memory management.

  @returns a pointer to a variable-length string, or NULL on failure.
 */
struct string* string_new(const char *str, size_t block);

/**
  Free a variable-length string

  @param  s    String to free.
 */
void string_free(struct string *s);

/**
  Append a C-string to the end of a variable-length string

  The arguments are ordered according to the following mnemonic:

  @verbatim
  string_add(my_string, " and then some")
  // my_string += " and then some"
  @endverbatim

  @param  s     Variable-length string to append \a str to.
  @param  str   C-style (NULL-terminated) string to append.

  @returns 0 on success; non-zero on falure.
 */
int string_append(struct string *s, const char *str);

/**
  Append a single character to the end of a variable-length string

  The arguments are ordered according to the following mnemonic:

  @verbatim
  string_add(my_string, '!');
  // my_string += '!'
  @endverbatim

  @param  s   Variable-length string to append \a c to.
  @param  c   Character to append.

  @returns 0 on success; non-zero on failure.
 */
int string_append1(struct string *s, char c);

/**
  Interpolate variable references in a string

  Variable interpolation is performed according to the following rules:

  $([a-zA-Z][a-zA-Z0-9_-]*) is expanded to the string value stored in \a ctx
  under the key \\1.  For example, in the string "My name is $name", "$name"
  is taken as a reference to the value stored in \a ctx under the key "name".

  Complex key names are also supported, using the format ${([^}]+)}.  To given
  another example, the string "My name is ${person.name}" contains a reference
  to the value stored in \a ctx under the key "person.name".

  The caller must take care to make \a buf large enough to accommodate the
  expanded string.  If the buffer is not long enough, this function will only
  fill \a buf with the first \a len bytes of the interpolated result.

  @param   buf  Pre-allocated buffer to house the interpolated string.
  @param   len  Length of \a buf, in bytes.
  @param   src  Original string to interpolate.
  @param   ctx  Hash containing the variables for interpolation against.

  @returns 0 on success, non-zero on failure.
 */
int string_interpolate(char *buf, size_t len, const char *src, const struct hash *ctx);

#endif
