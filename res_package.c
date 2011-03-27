#include "res_package.h"
#include "report.h"
#include "pack.h"

#define RES_PACKAGE_PACK_PREFIX "res_package::"
#define RES_PACKAGE_PACK_OFFSET 13
/* Pack format for res_package structure:
     L - enforced
     a - name
     a - version
 */
#define RES_PACKAGE_PACK_FORMAT "Laa"

struct res_package* res_package_new(const char *key)
{
	struct res_package *rp;

	rp = xmalloc(sizeof(struct res_package));
	list_init(&rp->res);

	rp->enforced = 0;
	rp->different = 0;
	rp->version = NULL;

	if (key) {
		res_package_set_name(rp, key);
		rp->key = string("res_package:%s", key);
	} else {
		rp->key = NULL;
	}

	return rp;
}

void res_package_free(struct res_package *rp)
{
	if (rp) {
		list_del(&rp->res);

		free(rp->name);
		free(rp->version);

		free(rp->key);
	}

	free(rp);
}

int res_package_setattr(struct res_package *rp, const char *name, const char *value)
{
	if (strcmp(name, "name") == 0) {
		return res_package_set_name(rp, value);
	} else if (strcmp(name, "version") == 0) {
		return res_package_set_version(rp, value);
	} else if (strcmp(name, "installed") == 0) {
		return res_package_set_presence(rp, strcmp(value, "no"));
	}

	return -1;
}

int res_package_set_presence(struct res_package *rp, int presence)
{
	assert(rp);

	if (presence) {
		rp->enforced ^= RES_PACKAGE_ABSENT;
	} else {
		rp->enforced |= RES_PACKAGE_ABSENT;
	}

	return 0;
}

int res_package_set_name(struct res_package *rp, const char *name)
{
	assert(rp);

	free(rp->name);
	rp->name = strdup(name);

	return 0;
}

int res_package_set_version(struct res_package *rp, const char *version)
{
	assert(rp);

	free(rp->version);
	rp->version = strdup(version);

	return 0;
}

int res_package_stat(struct res_package *rp, const struct package_manager *mgr)
{
	assert(rp);
	assert(mgr);

	free(rp->installed);
	rp->installed = package_manager_query(mgr, rp->name);

	return 0;
}

struct report* res_package_remediate(struct res_package *rp, int dryrun, const struct package_manager *mgr)
{
	assert(rp);
	assert(mgr);

	struct report *report = report_new("Package", rp->name);
	char *action;

	if (res_package_enforced(rp, ABSENT)) {
		if (rp->installed) {
			action = string("uninstall package");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
			} else if (package_manager_remove(mgr, rp->name) == 0) {
				report_action(report, action, ACTION_SUCCEEDED);
			} else {
				report_action(report, action, ACTION_FAILED);
			}
		}

		return report;
	}

	if (!rp->installed) {
		action = (rp->version ? string("install package v%s", rp->version)
		                      : string("install package (latest version)"));

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (package_manager_install(mgr, rp->name, rp->version) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}

		return report;
	}

	if (rp->version && strcmp(rp->installed, rp->version) != 0) {
		action = string("upgrade to v%s", rp->version);

		if (dryrun) {
			report_action(report, action, ACTION_SKIPPED);
		} else if (package_manager_install(mgr, rp->name, rp->version) == 0) {
			report_action(report, action, ACTION_SUCCEEDED);
		} else {
			report_action(report, action, ACTION_FAILED);
		}

		return report;
	}

	return report;
}

int res_package_is_pack(const char *packed)
{
	return strncmp(packed, RES_PACKAGE_PACK_PREFIX, RES_PACKAGE_PACK_OFFSET);
}

char *res_package_pack(struct res_package *rp)
{
	return pack(RES_PACKAGE_PACK_PREFIX, RES_PACKAGE_PACK_FORMAT,
	            rp->enforced,
	            rp->name, rp->version);
}

struct res_package* res_package_unpack(const char *packed)
{
	struct res_package *rp = res_package_new(NULL);

	if (unpack(packed, RES_PACKAGE_PACK_PREFIX, RES_PACKAGE_PACK_FORMAT,
		&rp->enforced,
		&rp->name, &rp->version) != 0) {

		res_package_free(rp);
		return NULL;
	}

	rp->key = string("res_package:%s", rp->name);

	if (rp->version && !*(rp->version)) {
		/* treat "" as NULL */
		free(rp->version);
		rp->version = NULL;
	}

	return rp;
}

