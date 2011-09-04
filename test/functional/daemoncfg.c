/*
  Copyright 2011 James Hunt <james@jameshunt.us>

  This file is part of Clockwork.

  Clockwork is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Clockwork is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Clockwork.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>

#include "../../conf/parser.h"
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
