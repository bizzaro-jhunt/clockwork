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

#define DEFAULT_POLICYD_CONF        CW_ETC_DIR "/policyd.conf"
#define DEFAULT_CWA_CONF            CW_ETC_DIR "/cwa.conf"
#define DEFAULT_MANIFEST_POL        CW_ETC_DIR "/manifest.pol"
#define DEFAULT_SSL_CA_CERT_FILE    CW_ETC_DIR "/ssl/CA.pem"
#define DEFAULT_SSL_CRL_FILE        CW_ETC_DIR "/ssl/revoked.pem"
#define DEFAULT_SSL_CERT_FILE       CW_ETC_DIR "/ssl/cert.pem"
#define DEFAULT_SSL_REQUEST_FILE    CW_ETC_DIR "/ssl/request.pem"
#define DEFAULT_SSL_KEY_FILE        CW_ETC_DIR "/ssl/key.pem"
#define DEFAULT_SSL_REQUESTS_DIR    CW_ETC_DIR "/ssl/pending"
#define DEFAULT_SSL_CERTS_DIR       CW_ETC_DIR "/ssl/signed"

#define DEFAULT_POLICYD_LOCK_FILE   CW_VAR_DIR "/lock/policyd"
#define DEFAULT_POLICYD_PID_FILE    CW_VAR_DIR "/run/policyd.pid"

#define DEFAULT_GATHERER_DIR        CW_LIB_DIR "/gather.d"

#define DEFAULT_SERVER_NAME         "clockwork"
#define DEFAULT_SERVER_PORT         "7890"
#define DEFAULT_POLICYD_LISTEN      "0.0.0.0:" DEFAULT_SERVER_PORT

#define DEFAULT_RETAIN_DAYS         45

#define CACHED_FACTS_DIR            CW_CACHE_DIR "/facts"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "sha1.h"
#include "mem.h"
#include "gear/gear.h"

#endif
