#include "test.h"
#include "assertions.h"
#include "../clockwork.h"
#include "../resource.h"

void test_resource_callbacks()
{
	struct resource *res;
	char *key;

	test("RESOURCE: Resource Callbacks");
	res = resource_new("user", "user1");
	assert_not_null("resource_new allocates a resource structure", res);
	assert_not_null("resource_new allocates a res_* member", res->resource);

	key = resource_key(res);
	assert_str_eq("resource_key returns the appropriate key", "user:user1", key);
	xfree(key);

	resource_free(res);
}

void test_resource_deps()
{
	struct resource *a, *b, *c;

	test("RESOURCE: Dependencies");
	c = resource_new("group", "group1");
	b = resource_new("user",  "owner");
	a = resource_new("file",  "/path/to/file");

	assert_not_null("a is a valid resource", a);
	assert_not_null("b is a valid resource", b);
	assert_not_null("c is a valid resource", c);

	assert_int_ne("a does not depend on b (yet)", resource_depends_on(a, b), 0);
	assert_int_ne("a does not depend on c (yet)", resource_depends_on(a, c), 0);
	assert_int_ne("b does not depend on a", resource_depends_on(b, a), 0);
	assert_int_ne("b does not depend on c", resource_depends_on(b, c), 0);
	assert_int_ne("c does not depend on a", resource_depends_on(c, a), 0);
	assert_int_ne("c does not depend on b", resource_depends_on(c, b), 0);

#if 0
	dep = dependency_new("resource a", "resource b");
	assert_not_null("dependency_new allocates a structure", dep);
	assert_str_eq("dep->a is 'resource a'", dep->a, "resource a");
	assert_null("dep->resource_a is initially NULL", dep->resource_a);
	assert_str_eq("dep->b is 'resource b'", dep->b, "resource b");
	assert_null("dep->resource_b is initially NULL", dep->resource_b);

	dep->resource_a = a;
	dep->resource_b = b;
#endif

	assert_int_eq("Adding a dependency succeeds", resource_add_dependency(a, b), 0);
	assert_int_eq("a now depends on b", resource_depends_on(a, b), 0);
	assert_int_ne("a does not depend on c (yet)", resource_depends_on(a, c), 0);
	assert_int_ne("b does not depend on a", resource_depends_on(b, a), 0);
	assert_int_ne("b does not depend on c", resource_depends_on(b, c), 0);
	assert_int_ne("c does not depend on a", resource_depends_on(c, a), 0);
	assert_int_ne("c does not depend on b", resource_depends_on(c, b), 0);

	assert_int_eq("Adding a 2nd dependency succeeds", resource_add_dependency(a, c), 0);
	assert_int_eq("a still depends on b", resource_depends_on(a, b), 0);
	assert_int_eq("a now depends on c", resource_depends_on(a, c), 0);
	assert_int_ne("b does not depend on a", resource_depends_on(b, a), 0);
	assert_int_ne("b does not depend on c", resource_depends_on(b, c), 0);
	assert_int_ne("c does not depend on a", resource_depends_on(c, a), 0);
	assert_int_ne("c does not depend on b", resource_depends_on(c, b), 0);

	assert_int_eq("Removing dependency succeeds", resource_drop_dependency(a, b), 0);
	assert_int_ne("a no longer depends on b", resource_depends_on(a, b), 0);
	assert_int_eq("a still depends on c", resource_depends_on(a, c), 0);
	assert_int_ne("b does not depend on a", resource_depends_on(b, a), 0);
	assert_int_ne("b does not depend on c", resource_depends_on(b, c), 0);
	assert_int_ne("c does not depend on a", resource_depends_on(c, a), 0);
	assert_int_ne("c does not depend on b", resource_depends_on(c, b), 0);

	assert_int_eq("Removing last dependency succeeds", resource_drop_dependency(a, c), 0);
	assert_int_ne("a no longer depends on b", resource_depends_on(a, b), 0);
	assert_int_ne("a no longer depends on c", resource_depends_on(a, c), 0);
	assert_int_ne("b does not depend on a", resource_depends_on(b, a), 0);
	assert_int_ne("b does not depend on c", resource_depends_on(b, c), 0);
	assert_int_ne("c does not depend on a", resource_depends_on(c, a), 0);
	assert_int_ne("c does not depend on b", resource_depends_on(c, b), 0);

	assert_int_ne("Dropping non-existent dependency fails", resource_drop_dependency(c, b), 0);

	resource_free(a);
	resource_free(b);
	resource_free(c);
}

void test_resource_unknown()
{
	struct resource *r;

	test("RESOURCE: Unknown Resource Type");
	r = resource_new("nonexistent_resource", "DNF");
	assert_null("resource_new wont create a resource of UNKNOWN type", r);
}

void test_resource_dependency_pack()
{
	struct dependency *d;
	const char *expected;
	char *packed;

	test("RESOURCE: dependency serialization");
	d = dependency_new("res.a", "res.b");
	assert_not_null("created dependency object", d);
	assert_str_eq("d->a is 'res.a'", "res.a", d->a);
	assert_str_eq("d->b is 'res.b'", "res.b", d->b);

	packed = dependency_pack(d);
	expected = "dependency::\"res.a\"\"res.b\"";
	assert_str_eq("Depenency packed properly", expected, packed);

	dependency_free(d);
	free(packed);
}

void test_resource_dependency_unpack()
{
	struct dependency *d;
	const char *packed;

	test("RESOURCE: dependency unserialization");
	packed = "dependency::\"file1\"\"user2\"";

	assert_null("dependency_unpack returns NULL on bad data", dependency_unpack("<invalid pack data>"));
	d = dependency_unpack(packed);
	assert_not_null("dependency_unpack returned a dependency object", d);
	assert_str_eq("d->a is 'file1'", "file1", d->a);
	assert_str_eq("d->b is 'user2'", "user2", d->b);
	assert_null("d->resource_a is NULL", d->resource_a);
	assert_null("d->resource_b is NULL", d->resource_b);

	dependency_free(d);
}

void test_suite_resource()
{
	test_resource_callbacks();
	test_resource_deps();
	test_resource_unknown();

	test_resource_dependency_pack();
	test_resource_dependency_unpack();
}
