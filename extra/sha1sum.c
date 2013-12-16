/*
  Copyright 2011-2013 James Hunt <james@niftylogic.com>

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
#include "sha1.h"
#include "mem.h"

int main(int argc, char **argv)
{
	struct SHA1 cksum;
	char *err;

	while (*++argv) {
		if (sha1_file(*argv, &cksum) == -1) {
			err = string("sha1sum: %s", *argv);
			perror(err);
			free(err);
		} else {
			printf("%s %s\n", cksum.hex, *argv);
		}
	}

	return 0;
}
