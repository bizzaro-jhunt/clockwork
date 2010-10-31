#ifndef _PACK_H
#define _PACK_H

#include <stdlib.h>

size_t pack(char *dst, size_t len, const char *format, ...);
int unpack(const char *packed, const char *format, ...);

#endif /* _PACK_H */
