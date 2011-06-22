#include "clockwork.h"
#include "augcw.h"

#ifdef DEVEL
#  define AUGCW_ROOT "test/augeas"
#  define AUGCW_INC  "augeas/lenses"
#else
#  define AUGCW_ROOT "/"
/** FIXME: what if lenses aren't in /var/clockwork? */
#  define AUGCW_INC  "/var/clockwork/augeas/lenses"
#endif

#define AUGCW_OPTS AUG_NO_STDINC|AUG_NO_LOAD|AUG_NO_MODL_AUTOLOAD

augeas* augcw_init(void)
{
	augeas *au;

	DEBUG("[augeas]: /files mapped to %s", AUGCW_ROOT);
	DEBUG("[augeas]: loadpath is %s",      AUGCW_INC);
	au = aug_init(AUGCW_ROOT, AUGCW_INC, AUGCW_OPTS);
	if (!au) {
		CRITICAL("augeas initialization failed");
		return NULL;
	}

	if (aug_set(au, "/augeas/load/Hosts/lens", "Hosts.lns") < 0
	 || aug_set(au, "/augeas/load/Hosts/incl", "/etc/hosts") < 0
	 || aug_load(au) != 0) {

		CRITICAL("augeas load failed");
		augcw_errors(au);
		aug_close(au);
		return NULL;
	}

	return au;
}

void augcw_errors(augeas* au)
{
	assert(au);

	char **results = NULL;
	const char *value;
	int rc, i;

	rc = aug_match(au, "/augeas//error", &results);
	if (rc == 0) { return; }

	INFO("%u Augeas errors found\n", rc);
	for (i = 0; i < rc; i++) {
		aug_get(au, results[i], &value);
		DEBUG(  "[epath]: %s", results[i]);
		WARNING("[error]: %s", value);
	}
	free(results);
}

stringlist* augcw_getm(augeas *au, const char *pathexpr)
{
	stringlist *values;
	int rc, i;
	char **results;
	const char *value;

	rc = aug_match(au, pathexpr, &results);
	values = stringlist_new(NULL);
	for (i = 0; i < rc; i++) {
		if (aug_get(au, results[i], &value) != 1
		 || stringlist_add(values, value) != 0) {

			stringlist_free(values);
			free(results);
			return NULL;
		}
	}
	free(results);

	return values;
}
