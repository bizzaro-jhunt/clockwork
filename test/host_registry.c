#include "test.h"
#include "assertions.h"
#include "../host_registry.h"

void test_host_entry_init_deinit()
{
	struct host_entry *ent;

	test("host_registry: host_entry initialization");
	ent = host_entry_new("localhost.localdomain.int", "127.0.0.1", "::1");
	assert_not_null("host_entry_new returns a valid host_entry pointer", ent);
	assert_str_equals("FQDN is set properly", "localhost.localdomain.int", ent->fqdn);
	assert_true("New host_entry does not belong to any lists", list_empty(&ent->registry));

	host_entry_free(ent);
}

void test_host_registry_init_deinit()
{
	struct host_registry *reg;

	test("host_registry: host_registry initialization");
	reg = host_registry_new();
	assert_not_null("host_registry_new returns a valid host_registry pointer", reg);
	assert_true("New host_registry has no hosts", list_empty(&reg->hosts));

	host_registry_free(reg);
}

void test_host_registry_adding_entries()
{
	struct host_registry *reg;
	struct host_entry *host1;
	struct host_entry *host2;
	struct host_entry *ptr;

	test("host_registry: adding host entries");
	reg = host_registry_new();
	host1 = host_entry_new("host21.example.net", "10.0.0.21", "::FFFF:10.0.0.21");
	host2 = host_entry_new("host22.example.net", "10.0.0.22", "::FFFF:10.0.0.22");
	assert_not_null("host_registry_new returns a valid host_registry pointer", reg);
	assert_not_null("host1 is a valid host_entry_pointer", host1);
	assert_not_null("host2 is a valid host_entry_pointer", host2);

	assert_int_equals("adding host1 to registry should succeed", 0, host_registry_add_entry(reg, host1));
	assert_true("host_registry->hosts should not be empty",    !list_empty(&reg->hosts));
	assert_true("host1->registry should not be an empty list", !list_empty(&host1->registry));

	assert_int_equals("adding host2 to registry should succeed", 0, host_registry_add_entry(reg, host2));
	assert_true("host_registry->hosts should not be empty",    !list_empty(&reg->hosts));
	assert_true("host2->registry should not be an empty list", !list_empty(&host2->registry));

	assert_int_equals("adding third host to registry should succeed", 0,
		host_registry_add(reg, "host23.example.net", "10.0.0.23", "::FFFF:10.0.0.23"));
	ptr = list_node(reg->hosts.prev, struct host_entry, registry);
	assert_not_null("Retrieving tail of host_registry->hosts list", ptr);
	assert_str_equals("last host added was in fact host23", "host23.example.net", ptr->fqdn);

	host_registry_free(reg);
}

void test_host_registry_find_by_name()
{
	struct host_registry *reg;
	struct host_entry *ent;

	test("host_registry: finding host entries in a registry");
	reg = host_registry_new();
	assert_not_null("host_registry_new returns a valid host_registry pointer", reg);
	assert_int_equals("Adding apple.example.net", 0,
		host_registry_add(reg, "apple.example.net", "192.168.1.101", "::FFFF:192.167.1.101"));
	assert_int_equals("Adding banana.example.net", 0,
		host_registry_add(reg, "banana.example.net", "192.168.1.102", "::FFFF:192.167.1.102"));
	assert_int_equals("Adding currant.example.net", 0,
		host_registry_add(reg, "currant.example.net", "192.168.1.103", "::FFFF:192.167.1.103"));

	ent = host_entry_find(reg, host_entry_find_by_fqdn, "banana.example.net");
	assert_not_null("Found banana.example.net in the registry", ent);
	assert_str_equals("Found the right node", "banana.example.net", ent->fqdn);

	ent = host_entry_find(reg, host_entry_find_by_fqdn, "tomato.example.net");
	/* tomato is not a fruit! */
	assert_null("Search for non-fruit tomato fails", ent);

	host_registry_free(reg);
}

void test_suite_host_registry()
{
	test_host_entry_init_deinit();
	test_host_registry_init_deinit();

	test_host_registry_adding_entries();
	test_host_registry_find_by_name();
}
