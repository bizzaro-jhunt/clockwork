#ifndef _TEST_ASSERTIONS_H
#define _TEST_ASSERTIONS_H

#include <sys/types.h>
#include <stdarg.h>

#include "../stringlist.h"

void assert_stringlist(stringlist *sl, const char *name, size_t n, ...);

#endif
