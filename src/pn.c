#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <syslog.h>

#include "pendulum.h"
#include "pendulum_funcs.h"

int main(int argc, char **argv)
{
	if (argc != 2) {
		fprintf(stderr, "USAGE: %s /path/to/script.pn\n", argv[0]);
		return 1;
	}

	FILE *io = fopen(argv[1], "r");
	if (!io) {
		fprintf(stderr, "Failed to open %s: %s\n",
				argv[1], strerror(errno));
		return 2;
	}

	openlog("pn", 0, LOG_DAEMON);

	pn_machine m;
	pn_init(&m);

	/* register clockwork pendulum functions */
	pendulum_funcs(&m, NULL);

	/* parse and run the input script */
	pn_parse(&m, io);
	pn_run(&m);

	closelog();
	return 0;
}
