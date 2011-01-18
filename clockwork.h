#ifndef _CLOCKWORK_H
#define _CLOCKWORK_H

#define _GNU_SOURCE

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

/** encrypted system group database **/
#ifndef SYS_GSHADOW
#define SYS_GSHADOW "/etc/gshadow"
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "sha1.h"
#include "log.h"
#include "mem.h"

#endif
