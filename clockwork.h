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

#ifndef CLOCKWORK_H
#define CLOCKWORK_H

#define CLOCKWORK_VERSION "0.2.3"

#define _GNU_SOURCE

/* cleartext system password database **/
#ifndef SYS_PASSWD
#define SYS_PASSWD "/etc/passwd"
#endif

/* encrypted system password database **/
#ifndef SYS_SHADOW
#define SYS_SHADOW "/etc/shadow"
#endif

/* cleartext system group database **/
#ifndef SYS_GROUP
#define SYS_GROUP "/etc/group"
#endif

/* encrypted system group database **/
#ifndef SYS_GSHADOW
#define SYS_GSHADOW "/etc/gshadow"
#endif

#ifdef DEVEL
#  define clockwork_aug_init() aug_init( \
	"test/augeas/", "augeas/lenses", \
	AUG_NO_STDINC|AUG_NO_LOAD|AUG_NO_MODL_AUTOLOAD)
#else
#  define clockwork_aug_init() aug_init( \
	"/", "/var/clockwork/augeas/lenses", \
	AUG_NO_STDINC|AUG_NO_LOAD|AUG_NO_MODL_AUTOLOAD)
#endif

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <gear.h>

#include "sha1.h"
#include "mem.h"

#endif
