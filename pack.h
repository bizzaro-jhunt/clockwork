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
  buf = pack("", "c", 255);   // buf == "ff"
  buf = pack("", "s", 48879); // buf == "beef"
  @endverbatim

  Signed integers (8-, 16- and 32-bit) are represented as hex
  two's complements:

  @verbatim
  buf = pack("C", -42);   // buf == "d6" (11010110b / 214d)
  @endverbatim

 */

/**
  Packs values into a string representation.

  The string buffer holding the packed representation will be dynamically
  allocated to be just large enough.  It is your responsibility to free
  it when you are done with it.

  @param  prefix    String to prepend to packed data.
  @param  format    Pack format string.
  @param  ...       Variadic list of values; agrees with \a format
                    in both type and number.

  @returns pointer to a dynamically-allocated buffer containing the
           packed data (including the prefix), or NULL on failure.
 */
char* pack(const char *prefix, const char *format, ...);

/**
  Unpacks a string representation into a set of value pointers.

  @param  packed    Packed string representation to unpack.
  @param  prefix    Prefix required for unpacking to continue.
  @param  format    Pack format string.
  @param  ...       Variadic list of pointers to values; agrees
                    with \a format in both type and number.

  @returns 0 on success, or non-zero on failure.
 */
int unpack(const char *packed, const char *prefix, const char *format, ...);

#endif /* _PACK_H */
