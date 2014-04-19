#ifndef T_TEST_H
#define T_TEST_H

#include <ctap.h>
#include <stdlib.h>
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
		if (streq(dep->a, a) && streq(dep->b, b)) {
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
