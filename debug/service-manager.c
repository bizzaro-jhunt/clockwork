#include "../clockwork.h"
#include "../managers/service.h"

int main(int argc, char **argv)
{
	int rc;
	const char *service;
	const struct service_manager* SM = SM_debian;

	if (argc != 3) {
		fprintf(stderr, "Usage: %s SERVICE (start|stop|restart|reload|status|enable|disable)\n",
		                argv[0]);
		return 1;
	}

	service = argv[1];
	printf("SERVICE: %s (%s)\n", service,
	       (service_enabled(SM, service) == 0 ? "enabled" : "disable"));

	if (strcmp(argv[2], "start") == 0) {
		printf("starting service '%s'... ", service);
		rc = service_start(SM, service);
		printf("%s (%i)\n", (rc == 0 ? "OK" : "FAILED"), rc);

		if (rc != 0) { return 3; }
		sleep(1);
		rc = service_running(SM, service);

	} else if (strcmp(argv[2], "stop") == 0) {
		printf("stopping service '%s'... ", service);
		rc = service_stop(SM, service);
		printf("%s (%i)\n", (rc == 0 ? "OK" : "FAILED"), rc);

		if (rc != 0) { return 3; }
		sleep(1);
		rc = service_running(SM, service);

	} else if (strcmp(argv[2], "restart") == 0) {
		printf("restarting service '%s'... ", service);
		rc = service_restart(SM, service);
		printf("%s (%i)\n", (rc == 0 ? "OK" : "FAILED"), rc);

		if (rc != 0) { return 3; }
		sleep(1);
		rc = service_running(SM, service);

	} else if (strcmp(argv[2], "reload") == 0) {
		printf("reloading service '%s'... ", service);
		rc = service_reload(SM, service);
		printf("%s (%i)\n", (rc == 0 ? "OK" : "FAILED"), rc);

		if (rc != 0) { return 3; }
		sleep(1);
		rc = service_running(SM, service);

	} else if (strcmp(argv[2], "status") == 0) {
		rc = service_running(SM, service);

	} else if (strcmp(argv[2], "enable") == 0) {
		printf("enabling service '%s'... ", service);
		rc = service_enable(SM, service);
		printf("%s (%i)\n", (rc == 0 ? "OK" : "FAILED"), rc);
		return rc;

	} else if (strcmp(argv[2], "disable") == 0) {
		printf("disabling service '%s'... ", service);
		rc = service_disable(SM, service);
		printf("%s (%i)\n", (rc == 0 ? "OK" : "FAILED"), rc);
		return rc;

	} else {
		fprintf(stderr, "Usage: svcmgr SERVICE (start|stop|restart|reload|status)\n");
		return 1;
	}

	if (rc == 0) {
		printf("%s: running\n", service);
	} else if (rc == 1) {
		printf("%s: not running\n", service);
	} else {
		printf("%s: unknown (rc %i)\n", service, rc);
	}

	return 0;
}
