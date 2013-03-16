/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

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
#include "../../clockwork.h"
#include "../../resource.h"

NEW_TEST(resource_callbacks)
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

	/* safe to call resource_notify on a res_user; it is a NOOP */
	assert_int_eq("resource_notify returns 0 for res_user (NOOP)",
		resource_notify(res, NULL), 0);

	resource_free(res);
}

NEW_TEST(resource_attrs_callback)
{
	struct resource *res;
	struct hash *h;

	test("RESOURCE: Attribute Retrieval via Callback");
	res = resource_new("host", "localhost");

	resource_set(res, "ip", "127.0.0.1");
	h = resource_attrs(res);
	assert_not_null("retrieved attributes", h);
	assert_str_eq("r->hostname is \"localhost\"", hash_get(h, "hostname"), "localhost");
	assert_str_eq("r->ip is \"127.0.0.1\"", hash_get(h, "ip"), "127.0.0.1");
	assert_null("r->aliases is NULL (unset)", hash_get(h, "aliases"));

	hash_free(h);
	resource_free(res);
}

NEW_TEST(resource_stat_fixup_callback)
{
	struct resource *res;
	struct res_dir *rd;
	struct resource_env env;

	struct report *report;

	const char *path = TEST_UNIT_DATA "/resource/dir";
	struct stat st;

	test("RESOURCE: Stat via Callback");
	if (stat(path, &st) != 0) {
		assert_fail("Unable to stat directory (pre-callback)");
		return;
	}
	assert_int_ne("Pre-stat mode is not 0705", st.st_mode & 07777, 0705);

	res = resource_new("dir", path);
	assert_not_null("res is not NULL", res);
	resource_set(res, "mode", "0705");

	assert_int_eq("resource_stat succeeds", resource_stat(res, &env), 0);
	rd = (struct res_dir *)(res->resource);
	assert_not_null("res->resource is not NULL (cast)", rd);
	assert_true("Directory exists", rd->exists);

	assert_true("Post-stat, mode is out of compliance", DIFFERENT(rd, RES_DIR_MODE));

	test("RESOURCE: Fixup via Callback");
	report = resource_fixup(res, 0, &env);
	assert_not_null("resource_fixup returns a report", report);

	if (stat(path, &st) != 0) {
		assert_fail("Unable to stat directory (post-fixup)");
		return;
	}
	assert_int_eq("Post-fixup, mode is 0705", st.st_mode & 07777, 0705);
	assert_int_eq("resource was fixed", report->fixed, 1);
	assert_int_eq("resource is now compliant", report->compliant, 1);

	resource_free(res);
	report_free(report);
}

NEW_TEST(resource_deps)
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

	test("RESOURCE: dependency_free(NULL)");
	dependency_free(NULL);
	assert_true("dependency_free(NULL) does not segfault", 1);

	resource_free(a);
	resource_free(b);
	resource_free(c);
}

NEW_TEST(resource_unknown)
{
	struct resource *r;

	test("RESOURCE: Unknown Resource Type");
	r = resource_new("nonexistent_resource", "DNF");
	assert_null("resource_new wont create a resource of UNKNOWN type", r);

	r = resource_unpack("no-such-resource::\"key\"00000001");
	assert_null("resource_unpack returns NULL on UNKNOWN type", r);
}

NEW_TEST(resource_dependency_pack)
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

NEW_TEST(resource_dependency_unpack)
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

NEW_TEST(resource_free_NULL)
{
	test("RESOURCE: resource_free(NULL)");
	resource_free(NULL);
	assert_true("resource_free(NULL) doesn't segfault", 1);
}

NEW_TEST(dependency_free_NULL)
{
	test("RESOURCE: dependency_free(NULL)");
	dependency_free(NULL);
	assert_true("dependency_free(NULL) doesn't segfault", 1);
}

NEW_SUITE(resource)
{
	RUN_TEST(resource_callbacks);
	RUN_TEST(resource_attrs_callback);
	RUN_TEST(resource_stat_fixup_callback);
	RUN_TEST(resource_deps);
	RUN_TEST(resource_unknown);

	RUN_TEST(resource_dependency_pack);
	RUN_TEST(resource_dependency_unpack);

	RUN_TEST(resource_free_NULL);
	RUN_TEST(dependency_free_NULL);
}
