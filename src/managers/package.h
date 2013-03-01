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

#ifndef MANAGERS_PACKAGE_H
#define MANAGERS_PACKAGE_H

#include "../clockwork.h"

typedef int (*package_manager_query_f)(const char *package, const char *version);
typedef char* (*package_manager_version_f)(const char *package);
typedef char* (*package_manager_latest_f)(const char *package);
typedef int (*package_manager_install_f)(const char *package, const char *version);
typedef int (*package_manager_remove_f)(const char *package);

struct package_manager {
	package_manager_query_f   query;
	package_manager_version_f version;
	package_manager_latest_f  latest;
	package_manager_install_f install;
	package_manager_remove_f  remove;
};

extern const struct package_manager *PM_dpkg_apt;
extern const struct package_manager *PM_rpm_yum;

#define package_installed(pm,package,version)  ((*((pm)->query))((package),(version)))
#define package_version(pm,package)            ((*((pm)->version))(package))
#define package_latest(pm,package)             ((*((pm)->latest))(package))
#define package_install(pm,package,version)    ((*((pm)->install))((package),(version)))
#define package_remove(pm,package)             ((*((pm)->remove))((package)))

#endif
