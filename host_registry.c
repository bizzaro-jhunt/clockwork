#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "host_registry.h"
#include "mem.h"

int host_entry_find_by_fqdn(const struct host_entry *ent, const void *needle)
{
	assert(ent);
	assert(needle);

	const char *fqdn = (const char *)(needle);
	return strcmp(ent->fqdn, fqdn) == 0 ? 0 : -1;
}

int host_entry_find_by_ipv4(const struct host_entry *ent, const void *needle)
{
	assert(ent);
	assert(needle);

	struct in_addr *ipv4 = (struct in_addr*)(needle);
	return ipv4->s_addr == ent->ipv4.s_addr ? 0 : -1;
}

int host_entry_find_by_ipv6(const struct host_entry *ent, const void *needle)
{
	assert(ent);
	assert(needle);

	struct in6_addr *ipv6 = (struct in6_addr*)(needle);
	return memcmp(ipv6->s6_addr, ent->ipv6.s6_addr, 16) == 0 ? 0 : -1;
}

struct host_registry* host_registry_new(void)
{
	struct host_registry *reg;

	reg = malloc(sizeof(struct host_registry));
	if (!reg) {
		return NULL;
	}

	list_init(&reg->hosts);

	return reg;
}

void host_registry_free(struct host_registry *reg)
{
	struct host_entry *ent, *tmp;

	/* FIXME: do we handle memory for hosts list? */

	for_each_node_safe(ent, tmp, &reg->hosts, registry) {
		host_entry_free(ent);
	}

	free(reg);
}

int host_registry_add(struct host_registry *reg, const char *fqdn, const char *ipv4, const char *ipv6)
{

	struct host_entry *ent;

	ent = host_entry_new(fqdn, ipv4, ipv6);
	if (!ent) {
		return -1;
	}

	return host_registry_add_entry(reg, ent);
}

int host_registry_add_entry(struct host_registry *reg, struct host_entry *ent)
{
	assert(reg);
	assert(ent);

	list_add_tail(&ent->registry, &reg->hosts);

	return 0;
}

struct host_entry* host_entry_new(const char *fqdn, const char *ipv4, const char *ipv6)
{
	struct host_entry *ent;

	ent = malloc(sizeof(struct host_entry));
	if (!ent) {
		return NULL;
	}

	list_init(&ent->registry);
	ent->fqdn = xstrdup(fqdn);

	memset(&ent->ipv4, 0, sizeof(struct in_addr));
	if (ipv4 && inet_pton(AF_INET, ipv4, &ent->ipv4) != 1) {
		host_entry_free(ent);
		return NULL;
	}

	memset(&ent->ipv6, 0, sizeof(struct in6_addr));
	if (ipv6 && inet_pton(AF_INET6, ipv6, &ent->ipv6) != 1) {
		host_entry_free(ent);
		return NULL;
	}

	return ent;
}

void host_entry_free(struct host_entry *ent)
{
	if (ent) {
		free(ent->fqdn);
		list_del(&ent->registry);
	}
	free(ent);
}

struct host_entry* host_entry_find(struct host_registry *reg, host_entry_filter f, void *needle)
{
	assert(reg);
	assert(f);
	assert(needle);

	struct host_entry *ent;

	for_each_node(ent, &reg->hosts, registry) {
		if ((*f)(ent, needle) == 0) {
			return ent;
		}
	}
	return NULL;
}
