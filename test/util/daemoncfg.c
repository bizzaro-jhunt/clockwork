#include <stdio.h>

#include "../../config/parser.h"
#include "../../hash.h"

/** daemoncfg - a test utility

  daemoncfg is responsible for parsing a daemon configuration file,
  extracting the value of a single directive, and printing it to
  standard output.
 */

int main(int argc, char **argv)
{
	struct hash *config;
	char *value;

	if (argc != 3) {
		printf("improper invocation!\n");
		return 1;
	}

	config = parse_config(argv[1]);
	if (!config) {
		printf("unable to parse file!\n");
		return 2;
	}

	value = hash_get(config, argv[2]);
	if (!value) {
		printf("USE DEFAULT\n");
	} else {
		printf("%s\n", value);
	}

	return 0;
}
