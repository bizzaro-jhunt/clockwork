#ifndef MANAGERS_PACKAGE_H
#define MANAGERS_PACKAGE_H

#include "../clockwork.h"

typedef int (*package_manager_query_f)(const char *package, const char *version);
typedef char* (*package_manager_version_f)(const char *package);
typedef int (*package_manager_install_f)(const char *package, const char *version);
typedef int (*package_manager_remove_f)(const char *package);

struct package_manager {
	package_manager_query_f   query;
	package_manager_version_f version;
	package_manager_install_f install;
	package_manager_remove_f  remove;
};

const struct package_manager *PM_dpkg_apt;
const struct package_manager *PM_rpm_yum;

#define package_installed(pm,package,version)  ((*((pm)->query))((package),(version)))
#define package_version(pm,package)            ((*((pm)->version))(package))
#define package_install(pm,package,version)    ((*((pm)->install))((package),(version)))
#define package_remove(pm,package)             ((*((pm)->remove))(package))

#endif
