/*
  Copyright 2011-2014 James Hunt <james@jameshunt.us>

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

#include "../src/cw.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

int main(int argc, char **argv)
{
	if (argc != 2) {
		printf("no lock specified!\n");
		exit(1);
	}

	int flags = getenv("TEST_LOCK_FALGS")
	          ? atoi(getenv("TEST_LOCK_FLAGS"))
	          : 0;

	cw_lock_t lock;
	cw_lock_init(&lock, argv[1]);
	if (cw_lock(&lock, flags) == 0) {
		printf("acquired %s\n", lock.path);
		exit(0);
	}

	printf("FAILED: %s in use by %s\n", lock.path, cw_lock_info(&lock));
	exit(1);
}
