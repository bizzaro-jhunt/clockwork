#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "clockwork.h"
#include "hash.h"
#include "conf/parser.h"

int main(int argc, char **argv)
{
	char *k; void *v;
	struct hash_cursor c;
	struct hash *config;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s /path/to/config\n", argv[0]);
		exit(1);
	}

	config = parse_config(argv[1]);
	for_each_key_value(config, &c, k, v) {
		printf("%s=\"%s\"\n", k, (char*)v);
	}
	hash_free_all(config);
	return 0;
}
