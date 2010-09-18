#ifndef MEM_H
#define MEM_H

#include <stdlib.h>

#define xfree(p) __xfree((void **)&(p))
void __xfree(void **ptr2ptr);

#endif /* MEM_H */
