/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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

TEST_HELPER
static void put_file(const char *path, int mode, const char *contents)
{
	FILE *io;

	if (!(io = fopen(path, "w")))
		BAIL_OUT(string("Unable to open %s for writing: %s", path, strerror(errno)));
	fprintf(io, "%s", contents);
	fchmod(fileno(io), mode);
	fclose(io);
}

TEST_HELPER
static FILE* temp_file(const char *contents)
{
	FILE *io;

	if (!(io = tmpfile()))
		BAIL_OUT(string("Unable to open temp file for writing: %s", strerror(errno)));
	fprintf(io, "%s", contents);
	rewind(io);
	return io;
}

#endif
