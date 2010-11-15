#ifndef _CONFIG_H
#define _CONFIG_H

#include "hash.h"

const char *config_ca_cert_file(struct hash *cfg);
const char *config_ca_key_file(struct hash *cfg);
const char *config_port(struct hash *cfg);
const char *config_lock_file(struct hash *cfg);
const char *config_pid_file(struct hash *cfg);
const char *config_manifest_file(struct hash *cfg);

#endif
