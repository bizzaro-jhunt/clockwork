#ifndef _AUGCW_H
#define _AUGCW_H

#include "clockwork.h"
#include "stringlist.h"

#include <augeas.h>

augeas* augcw_init(void);
void augcw_errors(augeas* au);
stringlist* augcw_getm(augeas *au, const char *pathexpr);

#endif
