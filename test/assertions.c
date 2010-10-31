#include "test.h"
#include "assertions.h"

void assert_stringlist(stringlist *sl, const char *name, size_t n, ...)
{
	va_list args;
	size_t i;
	char *str;
	char buf[128];

	snprintf(buf, 128, "%s has %d strings", name, n);
	assert_int_equals(buf, sl->num, n);

	va_start(args, n);
	for (i = 0; i < n; i++) {
		str = va_arg(args, char *);
		snprintf(buf, 128, "%s->strings[%d] == '%s'", name, i, str);
		assert_str_equals(buf, sl->strings[i], str);
	}
	va_end(args);

	snprintf(buf, 128, "%s->strings[%d] is NULL", name, sl->num);
	assert_null(buf, sl->strings[sl->num]);
}

