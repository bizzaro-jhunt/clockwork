#ifndef PKGMGR_H
#define PKGMGR_H

#include "clockwork.h"

struct package_manager {
	/** Command to query the package database for installed packages */
	char *query_local;
	/** Command to install the latest version of a package */
	char *install_latest;
	/** Command to install a specific version of a package */
	char *install_version;
	/** Command to remove an installed package */
	char *remove;
};

const struct package_manager PM_dpkg_apt;
#define DEBIAN_PACKAGE_MANAGER (&PM_dpkg_apt)
#define UBUNTU_PACKAGE_MANAGER (&PM_dpkg_apt)

const struct package_manager PM_rpm_yum;
#define REDHAT_PACKAGE_MANAGER (&PM_rpm_yum)
#define FEDORA_PACKAGE_MANAGER (&PM_rpm_yum)
#define CENTOS_PACKAGE_MANAGER (&PM_rpm_yum)
#define OEL_PACKAGE_MANAGER    (&PM_rpm_yum)

char* package_manager_query(const struct package_manager *mgr, const char *pkg);
int package_manager_install(const struct package_manager *mgr, const char *pkg, const char *version);
int package_manager_remove(const struct package_manager *mgr, const char *pkg);


#endif
