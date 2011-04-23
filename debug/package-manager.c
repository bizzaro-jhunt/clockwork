#include "../clockwork.h"
#include "../managers/package.h"

void usage(const char *command)
{
	fprintf(stderr, "Usage: %s (install|remove|query) PACKAGE [VERSION]\n",
	                command);
}

int main(int argc, char **argv)
{
	int rc;
	const char *action, *package, *version;
	const char *full;
	const struct package_manager* PM = PM_dpkg_apt;

	if (argc < 3 || argc > 4) {
		usage(argv[0]);
		return 1;
	}

	action = argv[1];
	package = argv[2];
	version = (argc == 4 ? argv[3] : NULL);
	full = version ? string("%s v%s", package, version)
	               : string("%s (latest)", package);

	rc = package_installed(PM, package, version);

	if (strcmp(action, "install") == 0) {
		if (rc == 0) {
			printf("Package '%s' already installed (version %s)\n",
			       package, package_version(PM, package));

			return 0;
		}

		rc = package_install(PM, package, version);
		if (rc != 0) {
			printf("Installation of package %s failed (rc: %u)\n", full, rc);
			return 2;
		}

		printf("Installed package %s\n", full);
		rc = package_installed(PM, package, version);
		if (rc == 0) {
			printf("Installation verified\n");
			return 0;
		} else {
			printf("Installation not verified.\n");
			return 3;
		}

	} else if (strcmp(action, "remove") == 0) {
		if (rc != 0) {
			printf("Package '%s' not installed\n", package);
			return 0;
		}

		rc = package_remove(PM, package);
		if (rc != 0) {
			printf("Removal of package %s failed (rc: %u)\n", full, rc);
			return 4;
		}

		printf("Removed package %s\n", full);
		rc = package_installed(PM, package, version);
		if (rc != 0) {
			printf("Removal verified.\n");
			return 0;
		} else {
			printf("Removal not verified.\n");
			return 5;
		}
	} else if (strcmp(action, "query") == 0) {
		if (rc == 0) {
			if (version) {
				printf("%s: installed\n", full);
			} else {
				printf("%s: installed (v%s)\n", full, package_version(PM, package));
			}
			return 0;
		} else {
			printf("%s: not installed\n", full);
			if (version && package_installed(PM, package, NULL) == 0) {
				printf("version %s *is* installed.\n", package_version(PM, package));
				return 0;
			}
		}
	} else {
		usage(argv[0]);
		return 1;
	}

	return 0;
}
