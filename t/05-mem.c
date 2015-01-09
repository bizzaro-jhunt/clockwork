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

#include "test.h"

TESTS {
	char *original[4] = {
		"string1",
		"another string",
		"a third and final string",
		NULL
	};
	char **copy;

	is_null(cw_arrdup(NULL), "cw_arrdup(NULL) == NULL");
	copy = NULL; cw_arrfree(copy);
	pass("cw_arrfree(NULL) doesn't segfault");

	copy = cw_arrdup(original);
	ok(copy != original, "different root pointer");
	is_string(copy[0], original[0], "copy[0] is a faithful copy");
	is_string(copy[1], original[1], "copy[1] is a faithful copy");
	is_string(copy[2], original[2], "copy[2] is a faithful copy");
	is_null(copy[3], "copy[3] is NULL");
	cw_arrfree(copy);


	done_testing();
}
