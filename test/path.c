#include "test.h"
#include "assertions.h"

#include "../path.h"

static void assert_canon(const char *orig_path, const char *expected)
{
	PATH *p;
	p = path_new(orig_path);
	assert_not_null("path_new succeeds", p);
	assert_int_eq("path_canon returns 0", path_canon(p), 0);
	assert_str_eq("path canonicalized properly", path(p), expected);
	path_free(p);
}

void test_path_creation()
{
	PATH *p;

	test("path: Creation of new paths");

	p = path_new(NULL);
	assert_null("NULL path string should yield NULL PATH", p);

	p = path_new("/etc/test");
	assert_not_null("Creation of new path succeeds", p);
	assert_str_eq("path buffer set properly", path(p), "/etc/test");

	path_free(p);
}

void test_path_canon()
{
	test("path: Canonicalization (normal case)");
	assert_canon("/usr/local/sbin", "/usr/local/sbin");

	test("path: Canonicalization (empty path)");
	assert_canon("", "");

	test("path: Canonicalization (single . component)");
	assert_canon("/usr/./local/sbin", "/usr/local/sbin");
	test("path: Canonicalization (multiple . components)");
	assert_canon("/usr/./././local/./sbin", "/usr/local/sbin");

	test("path: Canonicalization (single .. component)");
	assert_canon("/usr/local/opt/../sbin", "/usr/local/sbin");
	test("path: Canonicalization (multiple .. components)");
	assert_canon("/usr/share/man/../../root/../local/opt/../sbin", "/usr/local/sbin");

	test("path: Canonicalization (single /. at the end)");
	assert_canon("/usr/local/sbin/.", "/usr/local/sbin");
	test("path: Canonicalization (multiple /. at the end)");
	assert_canon("/usr/local/sbin/././.", "/usr/local/sbin");

	test("path: Canonicalization (single /./ at the end)");
	assert_canon("/usr/local/sbin/./", "/usr/local/sbin");
	test("path: Canonicalization (multiple /./ at the end)");
	assert_canon("/usr/local/sbin/./././", "/usr/local/sbin");

	test("path: Canonicalization (single /.. at the end)");
	assert_canon("/usr/local/sbin/..", "/usr/local");
	test("path: Canonicalization (multiple /.. at the end)");
	assert_canon("/usr/local/sbin/../..", "/usr");

	test("path: Canonicalization (single /../ at the end)");
	assert_canon("/usr/local/sbin/../", "/usr/local");
	test("path: Canonicalization (multiple /../ at the end)");
	assert_canon("/usr/local/sbin/../../", "/usr");

	test("path: Canonicalization (backup to /)");
	assert_canon("/usr/local/sbin/../../../etc", "/etc");
	test("path: Canonicalization (backup past /)");
	assert_canon("/etc/../../../", "/");

	test("path: Canonicalization (backup to / and stop)");
	assert_canon("/usr/local/sbin/../../../", "/");
	assert_canon("/usr/local/sbin/../../..",  "/");

	test("path: Canonicalization (stay at /)");
	assert_canon("/././././", "/");

	test("path: Canonicalization (free-for-all)");
	assert_canon("/etc/sysconfig/../.././etc/././../usr/local/./bin/../sbin", "/usr/local/sbin");

	test("path: Canonicalization (bogus paths)");
	assert_canon("/a/b/c/d/e/f/../", "/a/b/c/d/e");
}

void test_path_push_pop()
{
	PATH *p;

	test("path: path_pop");

	p = path_new("/var/clockwork/ssl/pending");
	assert_not_null("path_new succeeded", p);

	assert_int_ne("pop(1) returns non-zero (more to go)", path_pop(p), 0);
	assert_str_eq("pop(1) yields proper path", path(p), "/var/clockwork/ssl");

	assert_int_ne("pop(2) returns non-zero (more to go)", path_pop(p), 0);
	assert_str_eq("pop(2) yields proper path", path(p), "/var/clockwork");

	assert_int_ne("pop(3) returns non-zero (more to go)", path_pop(p), 0);
	assert_str_eq("pop(3) yields proper path", path(p), "/var");

	assert_int_eq("pop(4) returns non-zero (done)", path_pop(p), 0);

	test("path: path_push");

	assert_int_ne("push(1) returns non-zero (more to go)", path_push(p), 0);
	assert_str_eq("push(1) yields proper path", path(p), "/var/clockwork");

	assert_int_ne("push(2) returns non-zero (more to go)", path_push(p), 0);
	assert_str_eq("push(2) yields proper path", path(p), "/var/clockwork/ssl");

	assert_int_ne("push(3) returns non-zero (more to go)", path_push(p), 0);
	assert_str_eq("push(3) yields proper path", path(p), "/var/clockwork/ssl/pending");

	assert_int_eq("push(4) returns zero (done)", path_push(p), 0);

	path_free(p);
}

void test_path_free_null()
{
	PATH *p;

	test("path: path_free(NULL)");
	p = NULL;
	path_free(p);
	assert_null("path_free(NULL) doesn't segfault", p);
}

void test_suite_path()
{
	test_path_creation();
	test_path_canon();
	test_path_push_pop();
	test_path_free_null();
}
