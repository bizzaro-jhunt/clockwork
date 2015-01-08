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

#ifndef T_TEST_H
#define T_TEST_H

#include <ctap.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "../src/resource.h"
#include "../src/policy.h"

#ifdef __GNUC__
#define TEST_HELPER __attribute__ ((unused))
#else
#define TEST_HELPER
#endif

#ifndef TEST_DATA
#define TEST_DATA "t/data"
#endif

#ifndef TEST_TMP
#define TEST_TMP  "t/tmp"
#endif

TEST_HELPER
static int num_res(const struct policy *pol, enum restype t)
{
	int n = 0;
	struct resource *r;
	for_each_resource(r, pol) {
		if (r->type == t) { n++; }
	}
	return n;
}

TEST_HELPER
static int has_dep(const struct policy *pol, const char *a, const char *b)
{
	struct dependency *dep;
	for_each_dependency(dep, pol) {
		if (strcmp(dep->a, a) == 0 && strcmp(dep->b, b) == 0) {
			return 1;
		}
	}
	return 0;
}

TEST_HELPER
static void sys(const char *cmd)
{
	int rc = system(cmd);
	rc++;
}


#endif
