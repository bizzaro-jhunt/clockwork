#ifndef _HOST_REGISTRY_H
#define _HOST_REGISTRY_H

#include <netinet/in.h>

#include "list.h"

struct host_registry {
	struct list  hosts;
};

struct host_entry {
	struct list      registry;
	char            *fqdn;  /* Fully-qualified domain name */
	struct in_addr   ipv4;  /* IPv4 address structure */
	struct in6_addr  ipv6;  /* IPv6 address structure */
};

typedef int (*host_entry_filter)(const struct host_entry*, const void*);
int host_entry_find_by_fqdn(const struct host_entry*, const void*);
int host_entry_find_by_ipv4(const struct host_entry*, const void*);
int host_entry_find_by_ipv6(const struct host_entry*, const void*);

struct host_registry* host_registry_new(void);
void host_registry_free(struct host_registry *reg);

int host_registry_add(struct host_registry *reg, const char *fqdn, const char *ipv4, const char *ipv6);
int host_registry_add_entry(struct host_registry *reg, struct host_entry *ent);

struct host_entry* host_entry_new(const char *fqdn, const char *ipv4, const char *ipv6);
void host_entry_free(struct host_entry *ent);

struct host_entry* host_entry_find(struct host_registry *reg, host_entry_filter f, void *needle);

#endif
