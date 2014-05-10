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

#include "augcw.h"

#define AUGCW_OPTS AUG_NO_STDINC|AUG_NO_LOAD|AUG_NO_MODL_AUTOLOAD

augeas* augcw_init(void)
{
	augeas *au;

	cw_log(LOG_DEBUG, "[augeas]: /files mapped to %s", AUGEAS_ROOT);
	cw_log(LOG_DEBUG, "[augeas]: loadpath is %s",      AUGEAS_INCLUDES);
	au = aug_init(AUGEAS_ROOT, AUGEAS_INCLUDES, AUGCW_OPTS);
	if (!au) {
		cw_log(LOG_CRIT, "augeas initialization failed");
		return NULL;
	}

	if (aug_set(au, "/augeas/load/Hosts/lens", "Hosts.lns") < 0
	 || aug_set(au, "/augeas/load/Hosts/incl", "/etc/hosts") < 0
	 || aug_load(au) != 0) {

		cw_log(LOG_CRIT, "augeas load failed");
		augcw_errors(au);
		aug_close(au);
		return NULL;
	}

	return au;
}

void augcw_errors(augeas* au)
{
	assert(au); // LCOV_EXCL_LINE

	char **results = NULL;
	const char *value;
	int rc, i;

	rc = aug_match(au, "/augeas//error", &results);
	if (rc == 0) { return; }

	cw_log(LOG_INFO, "%u Augeas errors found\n", rc);
	for (i = 0; i < rc; i++) {
		aug_get(au, results[i], &value);
		cw_log(LOG_DEBUG,   "[epath]: %s", results[i]);
		cw_log(LOG_WARNING, "[error]: %s", value);
	}
	free(results);
}

struct stringlist* augcw_getm(augeas *au, const char *pathexpr)
{
	struct stringlist *values;
	int rc, i;
	char **results;
	const char *value;

	rc = aug_match(au, pathexpr, &results);
	values = stringlist_new(NULL);
	for (i = 0; i < rc; i++) {
		if (aug_get(au, results[i], &value) != 1
		 || stringlist_add(values, value) != 0) {

			stringlist_free(values);
			while (rc--) free(results[rc]);
			free(results);
			return NULL;
		}
	}
	while (rc--) free(results[rc]);
	free(results);

	return values;
}
