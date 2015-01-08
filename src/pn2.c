/*
  Copyright 2011-2015 James Hunt <james@jameshunt.us>

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

#include "vm.h"
#include <assert.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int main (int argc, char **argv)
{
	if (argc < 2) {
		fprintf(stderr, "USAGE: %s asm.b\n", argv[0]);
		return 1;
	}

	int rc, fd = open(argv[1], O_RDONLY);
	if (fd < 0) {
		perror(argv[1]);
		return 1;
	}
	off_t n = lseek(fd, 0, SEEK_END);
	if (n < 0) {
		perror(argv[1]);
		return 1;
	}
	if (n == 0) {
		fprintf(stderr, "%s: not a valid pendulum binary image\n", argv[1]);
		return 1;
	}
	byte_t *code = mmap(NULL, n, PROT_READ, MAP_PRIVATE, fd, 0);
	if (!code) {
		perror(argv[1]);
		return 1;
	}

	vm_t vm;
	rc = vm_reset(&vm);
	assert(rc == 0);

	rc = vm_prime(&vm, code, n);
	assert(rc == 0);

	rc = vm_args(&vm, argc, argv);
	assert(rc == 0);

	vm_exec(&vm);

	rc = vm_done(&vm);
	assert(rc == 0);

	return 0;
}
