/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

#ifndef CLOCKWORK_H
#define CLOCKWORK_H

#include "config.h"
#ifndef VERSION
#define VERSION PACKAGE_VERSION
#endif

#define _GNU_SOURCE

/* in development mode, don't touch the local system */
#ifdef DEVEL
#  define PREFIX "dev"
#else
#  define PREFIX
#endif

#ifndef SYS_AUTHDB_ROOT
#  define SYS_AUTHDB_ROOT PREFIX "/etc"
#endif

/* cleartext system password database **/
#ifndef SYS_PASSWD
#  define SYS_PASSWD PREFIX "/etc/passwd"
#endif

/* encrypted system password database **/
#ifndef SYS_SHADOW
#  define SYS_SHADOW PREFIX "/etc/shadow"
#endif

/* cleartext system group database **/
#ifndef SYS_GROUP
#  define SYS_GROUP PREFIX "/etc/group"
#endif

/* encrypted system group database **/
#ifndef SYS_GSHADOW
#  define SYS_GSHADOW PREFIX "/etc/gshadow"
#endif

#ifdef DEVEL
#  define AUGEAS_ROOT "test/augeas"
#else
#  define AUGEAS_ROOT "/"
#endif

#define CACHED_FACTS_DIR            CW_CACHE_DIR "/facts"

#ifndef CW_PAM_SERVICE
#  define CW_PAM_SERVICE "clockwork"
#endif

#ifndef CW_MTOOL_CONFIG_FILE
#  define CW_MTOOL_CONFIG_FILE "/etc/clockwork/cw.conf"
#endif

#ifndef CW_COGD_CONFIG_FILE
#  define CW_COGD_CONFIG_FILE "/etc/clockwork/cogd.conf"
#endif

#include "base.h"
#include <vigor.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "sha1.h"

#endif
