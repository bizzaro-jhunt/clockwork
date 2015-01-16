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

#ifndef __CW_H_
#define __CW_H_
#include <vigor.h>
#include <stdint.h>
#include <zmq.h>
#include <syslog.h> /* for LOG_* constants */
#include <sys/time.h>
#include <sys/types.h>
#include <pcre.h>

char* cw_strdup(const char *s);
int cw_strcmp(const char *a, const char *b);
char** cw_arrdup(char **a);
void cw_arrfree(char **a);

FILE* cw_tpl_erb(const char *src, hash_t *facts);

int cw_bdfa_pack(int out, const char *root);
int cw_bdfa_unpack(int in, const char *root);

int cw_authenticate(const char*, const char*, const char*);
const char *cw_autherror(void);

int cw_logio(int level, const char *fmt, FILE *io);

int cw_fcat(FILE *src, FILE *dst);
int cw_cat(int src, int dst);

#endif
