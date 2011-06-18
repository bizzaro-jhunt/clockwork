#include "test.h"
#include "assertions.h"

#include "../template.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>

#define TPL_FILE "test/tmp/working_template.tpl"

static int template_file(const char *path, const char *contents)
{
	int fd;
	size_t len = 0;
	ssize_t nwritten = 0;

	assert_not_null("template_file: file path is valid", path);
	assert_not_null("template_file: template string is valid", contents);
	if (!contents || !path) { goto failed; }

	fd = open(path, O_TRUNC|O_CREAT|O_WRONLY, 0644);
	assert_int_gte("template_file: handle opened successfully", fd, 0);
	if (fd < 0) { goto failed; }

	len = strlen(contents);
	do {
		nwritten = write(fd, contents, len);
		assert_int_gte("template_file: bytes written >= 0", nwritten, 0);
		if (nwritten < 0) { goto failed; }

		contents += nwritten;
		len -= nwritten;
	} while (len > 0 && nwritten > 0);

	close(fd);
	return 0;

failed:
	close(fd);
	return -1;
}

void test_template_var_expansion()
{
	struct hash *v;
	struct template *t;
	const char *tpl    = "v.test1 = <\%= v.test1 \%> and v.test2 = <\%= v.test2 \%>\n";
	const char *expect = "v.test1 = value1 and v.test2 = value2\n";
	char *actual;

	test("template: simple variable substitution");

	v = hash_new();
	assert_not_null("var hash is valid", v);
	hash_set(v, "v.test1", "value1");
	hash_set(v, "v.test2", "value2");

	assert_int_eq("Creation of template file succeeds",
		template_file(TPL_FILE, tpl), 0);

	t = template_create(TPL_FILE, v);
	assert_not_null("template created", t);

	actual = template_render(t);
	assert_not_null("template rendered", t);

	assert_str_eq("rendered value == expected value", actual, expect);

	hash_free(v);
	free(actual);
	template_free(t);
}

void test_template_if()
{
	struct hash *v;
	struct template *t;
	const char *tpl = "v1: <\% if v.v1 is \"1\" \%>one<\% else \%>not one<\% end \%>\n"
	                  "v2: <\% if v.v2 is not \"2\" \%>not two<\% else \%>two<\% end \%>\n";
	const char *expect = "v1: one\nv2: not two\n";
	char *actual;

	test("template: conditional if statements");

	v = hash_new();
	assert_not_null("var hash is valid", v);
	hash_set(v, "v.v1", "1");
	hash_set(v, "v.v2", "42");

	assert_int_eq("Creation of template file succeeds",
		template_file(TPL_FILE, tpl), 0);

	t = template_create(TPL_FILE, v);
	assert_not_null("template created", t);

	actual = template_render(t);
	assert_not_null("template rendered", t);

	assert_str_eq("rendered value == expected value", actual, expect);

	hash_free(v);
	free(actual);
	template_free(t);
}

void test_template_local_vars()
{
	struct hash *v;
	struct template *t;
	const char *tpl = "<\% local.var1 = \"World!\" \%>Hello, <\%= local.var1 \%>\n";
	const char *expect = "Hello, World!\n";
	char *actual;

	test("template: local variable expansion");

	v = hash_new();
	assert_not_null("var hash is valid", v);
	hash_set(v, "local.var1", "regression...");

	assert_int_eq("Creation of template file succeeds",
		template_file(TPL_FILE, tpl), 0);

	t = template_create(TPL_FILE, v);
	assert_not_null("template created", t);

	actual = template_render(t);
	assert_not_null("template rendered", t);

	assert_str_eq("rendered value == expected value", actual, expect);

	hash_free(v);
	free(actual);
	template_free(t);
}

void test_suite_template()
{
	test_template_var_expansion();
	test_template_if();
	test_template_local_vars();
}
