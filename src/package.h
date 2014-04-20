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

#ifndef PACKAGE_H
#define PACKAGE_H

#include "clockwork.h"

int package_query(const char *mgr, const char *pkg, const char *ver);
char *package_version(const char *mgr, const char *pkg);
char *package_latest(const char *mgr, const char *pkg);
int package_install(const char *mgr, const char *pkg, const char *ver);
int package_remove(const char *mgr, const char *pkg);

#endif
