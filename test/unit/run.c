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

int main(int argc, char **argv)
{
	TEST_SUITE(bits);
	TEST_SUITE(mem);
	TEST_SUITE(list);
	TEST_SUITE(stringlist);
	TEST_SUITE(hash);
	TEST_SUITE(userdb);
	TEST_SUITE(sha1);
	TEST_SUITE(pack);
	TEST_SUITE(resource);
	TEST_SUITE(resources);
	TEST_SUITE(res_user);
	TEST_SUITE(res_group);
	TEST_SUITE(res_file);
	TEST_SUITE(res_package);
	TEST_SUITE(res_service);
	TEST_SUITE(res_host);
	TEST_SUITE(res_sysctl);
	TEST_SUITE(res_dir);
	TEST_SUITE(policy);
	TEST_SUITE(fact);
	TEST_SUITE(stree);
	TEST_SUITE(cert);
	TEST_SUITE(string);
	TEST_SUITE(job);
	TEST_SUITE(proto);
	TEST_SUITE(template);
	TEST_SUITE(path);

	return run_tests(argc, argv);
}
