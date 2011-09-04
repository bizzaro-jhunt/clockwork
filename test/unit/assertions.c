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

#include "test.h"
#include "assertions.h"

void assert_stringlist(stringlist *sl, const char *name, size_t n, ...)
{
	va_list args;
	size_t i;
	char *str;
	char buf[128];

	snprintf(buf, 128, "%s has %d strings", name, n);
	assert_int_eq(buf, sl->num, n);

	va_start(args, n);
	for (i = 0; i < n; i++) {
		str = va_arg(args, char *);
		snprintf(buf, 128, "%s->strings[%d] == '%s'", name, i, str);
		assert_str_eq(buf, sl->strings[i], str);
	}
	va_end(args);

	snprintf(buf, 128, "%s->strings[%d] is NULL", name, sl->num);
	assert_null(buf, sl->strings[sl->num]);
}

void assert_policy_has(const char *msg, const struct policy *pol, enum restype t, int num)
{
	int n = 0;
	struct resource *r;
	assert(pol);

	for_each_resource(r, pol) {
		if (r->type == t) { n++; }
	}

	assert_int_eq(msg, num, n);
}

void assert_dep(const struct policy *pol, const char *a, const char *b)
{
	char msg[128];
	struct dependency *dep;

	snprintf(msg, 128, "'%s' should depend on '%s'", a, b);
	for_each_dependency(dep, pol) {
		if (streq(dep->a, a) && streq(dep->b, b)) {
			assert_true(msg, 1);
			return;
		}
	}
	assert_fail(msg);
}
