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

