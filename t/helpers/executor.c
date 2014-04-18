/*
  Copyright 2011-2014 James Hunt <james@niftylogic.com>

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

#include "../../src/clockwork.h"
#include "../../src/exec.h"

int main(int argc, char **argv)
{
	char *std_out, **fetch_stdout = NULL;
	char *std_err, **fetch_stderr = NULL;
	int rc;

	if (argc != 3) {
		fprintf(stderr, "USAGE: %s (stdout|stderr|none|both) 'command --args'\n", argv[0]);
		exit(1);
	}

	if (strcmp(argv[1], "stdout") == 0 || strcmp(argv[1], "both") == 0) {
		fetch_stdout = &std_out;
	}
	if (strcmp(argv[1], "stderr") == 0 || strcmp(argv[1], "both") == 0) {
		fetch_stderr = &std_err;
	}

	rc = exec_command(argv[2], fetch_stdout, fetch_stderr);
	printf("rc = %i\n", rc);
	printf("STDOUT\n");
	printf("------\n");
	if (fetch_stdout) {
		printf("%s", std_out);
	}
	printf("STDERR\n");
	printf("------\n");
	if (fetch_stderr) {
		printf("%s", std_err);
	}
	printf("------\n");

	exit(0);
}
