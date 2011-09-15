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

#include "test.h"

NEW_TEST(basic_twiddles)
{
	unsigned int flags = 0;

	flags |= 00001;
	flags |= 00010;
	flags |= 00020;
	flags |= 00040;
	assert_int_eq("001 | 010 | 020 | 040 == 071", 071, flags);

	flags |= 0020;
	assert_int_eq("setting 020 again still yields 071", 071, flags);

	flags &= ~00020;
	assert_int_eq("unsetting 020 yields 051", 051, flags);

	flags &= ~00020;
	assert_int_eq("unsetting 020 again still yields 051", 051, flags);
}

NEW_SUITE(bits)
{
	RUN_TEST(basic_twiddles);
}
