#ifndef T_TEST_H
#define T_TEST_H

#include "ctap.h"
#include "../src/resource.h"
#include "../src/policy.h"

static int num_res(const struct policy *pol, enum restype t)
{
	int n = 0;
	struct resource *r;
	for_each_resource(r, pol) {
		if (r->type == t) { n++; }
	}
	return n;
}

static int has_dep(const struct policy *pol, const char *a, const char *b)
{
	struct dependency *dep;
	for_each_dependency(dep, pol) {
		if (streq(dep->a, a) && streq(dep->b, b)) {
			return 1;
		}
	}
	return 0;
}

#endif
