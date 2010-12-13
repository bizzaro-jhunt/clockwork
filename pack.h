#ifndef _PACK_H
#define _PACK_H

#include <stdlib.h>

/** @file pack.h

  This module defines two functions, pack and unpack, that can be used
  to serialize complex objects and structures into simple ASCII strings,
  for long-term storage, network transmission, and more.

  These two functions work on the same principles as Perl's pack operator:
  the caller supplies a format string and a variable list of values (for
  pack) or pointers to values (for unpack).

  Here are the currently implemented format field specifiers:

  - a - Character string, NULL-terminated
  - c - Signed 8-bit integer (unsigned char)
  - C - Unsigned 8-bit integer (char)
  - s - Signed 16-bit integer (short)
  - S - Unsigned 16-bit integer (unsigned short)
  - l - Signed 32-bit integer (long)
  - L - Unsigned 32-bit integer (unsigned long).

  All integer formats (cCsSlL) are resprented in hex, without
  the leading '0x':

  @verbatim
  pack(&buf, 3, "c", 255);   // buf == "ff"
  pack(&buf, 5, "s", 48879); // buf == "beef"
  pack(&buf, 2, "c", 127);   // buf is undefined
                             //  (because 2 bytes is not enough space
                             //  to represent an 8-bit integer in hex,
                             //  with the NULL terminator.
  @endverbatim

  Signed integers (8-, 16- and 32-bit) are represented as hex
  two's complements:

  @verbatim
  pack(&buf, 3, "C", -42);   // buf == "d6" (11010110b / 214d)
  @endverbatim

  This function deliberately behaves like snprintf; the return
  value is the number of bytes required in the dst buffer to
  contain the complete packed representation.  This feature can
  be used to size a buffer exactly to the requirements:

  @verbatim
  size_t len;
  char *buf;
  len = pack(NULL, 0, "c", 255);
  buf = calloc(len, sizeof(char));
  pack(&buf, len, "c", 255);
  @endverbatim

 */

/**
  Packs values into a string representation.

  If \a dst is NULL, then this function goes through the motion
  of building the packed representation, but only returns the
  minimum number of bytes that needed in \a dst to fully
  contain the packed string.

  @param  dst       Character buffer to populate.
  @param  len       Maximum length of \a dst.
  @param  format    Pack format string.
  @param  ...       Variadic list of values; agrees with \a format
                    in both type and number.

  @returns the number of bytes copied into \a dst, or -1 on failure.
 */
size_t pack(char *dst, size_t len, const char *format, ...);

/**
  Unpacks a string representation into a set of value pointers.

  @param  packed    Packed string representation to unpack.
  @param  format    Pack format string.
  @param  ...       Variadic list of pointers to values; agrees
                    with \a format in both type and number.

  @returns 0 on success, or non-zero on failure.
 */
int unpack(const char *packed, const char *format, ...);

#endif /* _PACK_H */
