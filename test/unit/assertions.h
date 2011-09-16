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

#ifndef _TEST_ASSERTIONS_H
#define _TEST_ASSERTIONS_H

#include <sys/types.h>
#include <stdarg.h>

#include "../../clockwork.h"
#include "../../policy.h"

void assert_stringlist(struct stringlist *sl, const char *name, size_t n, ...);
void assert_policy_has(const char *msg, const struct policy *pol, enum restype t, int num);

#define assert_policy_has_users(msg,p,n)  assert_policy_has(msg,p,RES_USER, n)
#define assert_policy_has_groups(msg,p,n) assert_policy_has(msg,p,RES_GROUP,n)
#define assert_policy_has_files(msg,p,n)  assert_policy_has(msg,p,RES_FILE, n)

void assert_dep(const struct policy *pol, const char *a, const char *b);

#endif
