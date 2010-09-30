#ifndef ENV_H
#define ENV_H

#ifdef TEST
#include "test/env.h"
#endif

/** cleartext system password database **/
#ifndef SYS_PASSWD
#define SYS_PASSWD "/etc/passwd"
#endif

/** encrypted system password database **/
#ifndef SYS_SHADOW
#define SYS_SHADOW "/etc/shadow"
#endif

/** cleartext system group database **/
#ifndef SYS_GROUP
#define SYS_GROUP "/etc/group"
#endif

#endif
