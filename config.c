#include "config.h"

#define DEFAULT_port "7890"

static const char *_config(struct hash *cfg, const char *key, const char *def)
{
	const char *v;
	v = hash_get(cfg, key);
	return (v ? v : def);
}

#define CONFIG_DEFAULT(v,d) const char *config_ ## v (struct hash *c) { return _config(c, #v, d); }

CONFIG_DEFAULT(ca_cert_file,  "/etc/clockwork/ssl/CA.pem");
CONFIG_DEFAULT(ca_key_file,   "/etc/clockwork/ssl/CA.key");
CONFIG_DEFAULT(port,          "7890");
CONFIG_DEFAULT(lock_file,     "/var/lock/policyd");
CONFIG_DEFAULT(pid_file,      "/var/run/policyd");
CONFIG_DEFAULT(manifest_file, "/etc/clockwork/manifest.pol");

