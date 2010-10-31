#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "host_registry.h"
#include "mem.h"

int host_entry_find_by_fqdn(const struct host_entry *ent, const void *needle)
{
	assert(ent);
	assert(needle);

	const char *fqdn = (const char *)(needle);
	return strcmp(ent->fqdn, fqdn);
}

struct host_registry* host_registry_new(void)
{
	struct host_registry *reg;

	reg = malloc(sizeof(struct host_registry));
	if (!reg) {
		return NULL;
	}

	if (host_registry_init(reg) != 0) {
		free(reg);
		return NULL;
	}

	return reg;
}

int host_registry_init(struct host_registry *reg)
{
	assert(reg);

	list_init(&reg->hosts);

	return 0;
}

void host_registry_deinit(struct host_registry *reg)
{
	assert(reg);

	/* no-op for now; do we handle memory for hosts list? */
}

void host_registry_free(struct host_registry *reg)
{
	struct host_entry *ent, *tmp;

	host_registry_deinit(reg);

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

	if (host_entry_init(ent, fqdn, ipv4, ipv6) != 0) {
		free(ent);
		return NULL;
	}

	return ent;
}

int host_entry_init(struct host_entry *ent, const char *fqdn, const char *ipv4, const char *ipv6)
{
	assert(ent);

	ent->fqdn = strdup(fqdn);
	list_init(&ent->registry);

	return 0;
}

void host_entry_deinit(struct host_entry *ent)
{
	assert(ent);

	xfree(ent->fqdn);
	list_del(&ent->registry);
}

void host_entry_free(struct host_entry *ent)
{
	assert(ent);

	host_entry_deinit(ent);
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
