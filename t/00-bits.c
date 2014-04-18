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
	unsigned int f = 0;
	f |= 00001;
	f |= 00010;
	f |= 00020;
	f |= 00040;
	is_int(f, 071, "001 | 010 | 020 | 040 == 071");

	f |= 0020;
	is_int(f, 071, "setting 020 again still yields 071");

	f &= ~00020;
	is_int(f, 051, "unsetting 020 yields 051");

	f &= ~00020;
	is_int(f, 051, "unsetting 020 again still yields 051");

	done_testing();
}
