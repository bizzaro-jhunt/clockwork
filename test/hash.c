#include "test.h"
#include "assertions.h"
#include "../hash.h"

void test_hash_functions()
{
	test("hash: H64");

	assert_int_equals("H64 equivalency", H64("test"), H64("test"));
}

void test_hash_basics()
{
	struct hash *h;

	test("hash: Insertion and Lookup");
	assert_int_not_equal("no H64 collision", H64("path"), H64("name"));

	h = hash_new();
	assert_not_null("hash_new returns a pointer", h);

	assert_null("Search for 'path' fails prior to insertion", hash_lookup(h, "path"));
	assert_null("Search for 'name' fails prior to insertion", hash_lookup(h, "name"));

	assert_int_equals("insertion of 'path' succeeds", 0, hash_insert(h, "path", "/some/path/some/where"));
	assert_int_equals("insertion of 'name' succeeds", 0, hash_insert(h, "name", "staff"));

	assert_str_equals("Search for 'path' succeeds", "/some/path/some/where", hash_lookup(h, "path"));
	assert_str_equals("Search for 'name' succeeds", "staff", hash_lookup(h, "name"));

	hash_free(h);
}

void test_hash_collisions()
{
	struct hash *h;

	test("hash: Handling of Keyspace Collisions");
	assert_int_equals("H64 collision", H64("path"), H64("group"));

	h = hash_new();
	assert_not_null("hash_new returns a pointer", h);

	assert_null("Search for 'path' fails prior to insertion", hash_lookup(h, "path"));
	assert_null("Search for 'group' fails prior to insertion", hash_lookup(h, "group"));

	assert_int_equals("insertion of 'path' succeeds", 0, hash_insert(h, "path", "/some/path/some/where"));
	assert_int_equals("insertion of 'group' succeeds", 0, hash_insert(h, "group", "staff"));

	assert_str_equals("Search for 'path' succeeds", "/some/path/some/where", hash_lookup(h, "path"));
	assert_str_equals("Search for 'group' succeeds", "staff", hash_lookup(h, "group"));

	hash_free(h);
}

void test_hash_double_insert()
{
	struct hash *h;

	test("hash: Duplicate value insertion");

	h = hash_new();
	assert_not_null("hash_new returns a pointer", h);

	assert_int_equals("first insertion succeeds",  0, hash_insert(h, "key", "value1"));
	assert_int_not_equal("second insertion fails", 0, hash_insert(h, "key", "value2"));

	assert_str_equals("retrieval returns first value", "value1", hash_lookup(h, "key"));

	hash_free(h);
}

void test_suite_hash()
{
	test_hash_functions();
	test_hash_basics();
	test_hash_collisions();
	test_hash_double_insert();
}
