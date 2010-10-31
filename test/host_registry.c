#include <arpa/inet.h>

#include "test.h"
#include "assertions.h"
#include "../host_registry.h"

#define LOCAL_FQDN "localhost.localdomain"
#define LOCAL_IPV4 "127.0.0.1"
#define LOCAL_IPV6 "::1"

#define APPLE_FQDN "apple.example.net"
#define APPLE_IPV4 "192.168.1.101"
#define APPLE_IPV6 "::FFFF:192.168.1.101"

#define BANANA_FQDN "banana.example.net"
#define BANANA_IPV4 "192.168.1.102"
#define BANANA_IPV6 "::FFFF:192.168.1.102"

#define CURRANT_FQDN "currant.example.net"
#define CURRANT_IPV4 "192.168.1.103"
#define CURRANT_IPV6 "::FFFF:192.168.1.103"

#define TOMATO_FQDN "tomato.lab.example.net"
#define TOMATO_IPV4 "10.17.0.142"
#define TOMATO_IPV6 "::FFFF:10.17.0.142"

void test_host_entry_init_deinit()
{
	struct host_entry *ent;
	struct in_addr  ipv4;
	struct in6_addr ipv6;
	struct in6_addr null_ipv6;

	/* this test only deals with the local address (127.0.0.1) */
	inet_pton(AF_INET,  LOCAL_IPV4, &ipv4);
	inet_pton(AF_INET6, LOCAL_IPV6, &ipv6);
	inet_pton(AF_INET6, "::", &null_ipv6);

	test("host_registry: host_entry initialization");
	ent = host_entry_new(LOCAL_FQDN, LOCAL_IPV4, LOCAL_IPV6);
	assert_not_null("host_entry_new returns a valid host_entry pointer", ent);
	assert_str_equals("FQDN is set properly", LOCAL_FQDN, ent->fqdn);
	assert_unsigned_equals("IPv4 address is set properly", ipv4.s_addr, ent->ipv4.s_addr);
	assert_int_equals("IPv6 address is set properly", 0, memcmp(ipv6.s6_addr, ent->ipv6.s6_addr, 16));
	assert_true("New host_entry does not belong to any lists", list_empty(&ent->registry));
	host_entry_free(ent);

	test("host_registry: host_entry initialize with missing fqdn");
	ent = host_entry_new(NULL, LOCAL_IPV4, LOCAL_IPV6);
	assert_not_null("host_entry_new returns a valid host_entry pointer", ent);
	assert_null("FQDN is NULL", ent->fqdn);
	assert_unsigned_equals("IPv4 address is set properly", ipv4.s_addr, ent->ipv4.s_addr);
	assert_int_equals("IPv6 address is set properly", 0, memcmp(ipv6.s6_addr, ent->ipv6.s6_addr, 16));
	host_entry_free(ent);

	test("host_registry: host_entry initialize with missing IPv4 address");
	ent = host_entry_new(LOCAL_FQDN, NULL, LOCAL_IPV6);
	assert_not_null("host_entry_new returns a valid host_entry pointer", ent);
	assert_str_equals("FQDN is set properly", LOCAL_FQDN, ent->fqdn);
	assert_unsigned_equals("IPv4 address is 0.0.0.0", 0, ent->ipv4.s_addr);
	assert_int_equals("IPv6 address is set properly", 0, memcmp(ipv6.s6_addr, ent->ipv6.s6_addr, 16));
	host_entry_free(ent);

	test("host_registry: host_entry initialize with missing IPv6 address");
	ent = host_entry_new(LOCAL_FQDN, LOCAL_IPV4, NULL);
	assert_not_null("host_entry_new returns a valid host_entry pointer", ent);
	assert_str_equals("FQDN is set properly", LOCAL_FQDN, ent->fqdn);
	assert_unsigned_equals("IPv4 address is set properly", ipv4.s_addr, ent->ipv4.s_addr);
	assert_int_equals("IPv6 address is ::", 0, memcmp(null_ipv6.s6_addr, ent->ipv6.s6_addr, 16));
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

static struct host_registry* setup_host_finder_registry()
{
	struct host_registry *reg;

	reg = host_registry_new();
	assert_not_null("host_registry_new returns a valid host_registry pointer", reg);
	assert_int_equals("Adding " APPLE_FQDN, 0,
		host_registry_add(reg, APPLE_FQDN, APPLE_IPV4, APPLE_IPV6));
	assert_int_equals("Adding " BANANA_FQDN, 0,
		host_registry_add(reg, BANANA_FQDN, BANANA_IPV4, BANANA_IPV6));
	assert_int_equals("Adding " CURRANT_FQDN, 0,
		host_registry_add(reg, CURRANT_FQDN, CURRANT_IPV4, CURRANT_IPV6));

	return reg;
}

void test_host_registry_find_by_name()
{
	struct host_registry *reg;
	struct host_entry *ent;

	test("host_registry: finding host entries by name (fqdn)");
	reg = setup_host_finder_registry();

	ent = host_entry_find(reg, host_entry_find_by_fqdn, "banana.example.net");
	assert_not_null("Found " BANANA_FQDN " in the registry", ent);
	assert_str_equals("Found the right node", BANANA_FQDN, ent->fqdn);

	ent = host_entry_find(reg, host_entry_find_by_fqdn, TOMATO_FQDN);
	/* tomato is not a fruit! */
	assert_null("Search for " TOMATO_FQDN " fails", ent);

	host_registry_free(reg);
}

void test_host_registry_find_by_ipv4_address()
{
	struct host_registry *reg;
	struct host_entry *ent;
	struct in_addr ipv4;

	test("host_registry: finding host entries by ipv4 address");
	reg = setup_host_finder_registry();

	assert_int_equals("inet_pton(" BANANA_IPV4 ") should succeed", 1, inet_pton(AF_INET, BANANA_IPV4, &ipv4));
	ent = host_entry_find(reg, host_entry_find_by_ipv4, &ipv4);
	assert_not_null("Found " BANANA_IPV4 " / " BANANA_FQDN " in the registry", ent);
	if (ent) {
		assert_str_equals("Found the right node (fqdn check)", BANANA_FQDN, ent->fqdn);
	}

	assert_int_equals("inet_pton(" CURRANT_IPV4 ") should succeed", 1, inet_pton(AF_INET, CURRANT_IPV4, &ipv4));
	ent = host_entry_find(reg, host_entry_find_by_ipv4, &ipv4);
	assert_not_null("Found " CURRANT_IPV4 " / " CURRANT_FQDN " in the registry", ent);
	if (ent) {
		assert_str_equals("Found the right node (fqdn check)", CURRANT_FQDN, ent->fqdn);
	}

	ent = host_entry_find(reg, host_entry_find_by_ipv4, TOMATO_IPV4);
	/* tomato is not a fruit! */
	assert_null("Search for " TOMATO_IPV4" fails", ent);

	host_registry_free(reg);
}

void test_host_registry_find_by_ipv6_address()
{
	struct host_registry *reg;
	struct host_entry *ent;
	struct in6_addr ipv6;

	test("host_registry: finding host entries by ipv6 address");
	reg = setup_host_finder_registry();

	assert_int_equals("inet_pton(" BANANA_IPV6 ") should succeed", 1, inet_pton(AF_INET6, BANANA_IPV6, &ipv6));
	ent = host_entry_find(reg, host_entry_find_by_ipv6, &ipv6);
	assert_not_null("Found " BANANA_IPV6 " / " BANANA_FQDN " in the registry", ent);
	if (ent) {
		assert_str_equals("Found the right node (fqdn check)", BANANA_FQDN, ent->fqdn);
	}

	assert_int_equals("inet_pton(" CURRANT_IPV6 ") should succeed", 1, inet_pton(AF_INET6, CURRANT_IPV6, &ipv6));
	ent = host_entry_find(reg, host_entry_find_by_ipv6, &ipv6);
	assert_not_null("Found " CURRANT_IPV6 " / " CURRANT_FQDN " in the registry", ent);
	if (ent) {
		assert_str_equals("Found the right node (fqdn check)", CURRANT_FQDN, ent->fqdn);
	}

	ent = host_entry_find(reg, host_entry_find_by_ipv6, TOMATO_IPV6);
	/* tomato is not a fruit! */
	assert_null("Search for " TOMATO_IPV6" fails", ent);

	host_registry_free(reg);
}

void test_suite_host_registry()
{
	test_host_entry_init_deinit();
	test_host_registry_init_deinit();

	test_host_registry_adding_entries();
	test_host_registry_find_by_name();
	test_host_registry_find_by_ipv4_address();
	test_host_registry_find_by_ipv6_address();
}
