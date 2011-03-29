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

void* res_package_new(const char *key)
{
	struct res_package *rp;

	rp = xmalloc(sizeof(struct res_package));
	list_init(&rp->res);

	rp->enforced = 0;
	rp->different = 0;
	rp->version = NULL;

	if (key) {
		res_package_set(rp, "name", key);
		rp->key = string("res_package:%s", key);
	} else {
		rp->key = NULL;
	}

	return rp;
}

void res_package_free(void *res)
{
	struct res_package *rp = (struct res_package*)(res);
	if (rp) {
		list_del(&rp->res);

		free(rp->name);
		free(rp->version);

		free(rp->key);
	}

	free(rp);
}

const char* res_package_key(const void *res)
{
	const struct res_package *rp = (struct res_package*)(res);
	assert(rp);

	return rp->key;
}

int res_package_norm(void *res) { return 0; }

int res_package_set(void *res, const char *name, const char *value)
{
	struct res_package *rp = (struct res_package*)(res);
	if (strcmp(name, "name") == 0) {
		free(rp->name);
		rp->name = strdup(value);

	} else if (strcmp(name, "version") == 0) {
		free(rp->version);
		rp->version = strdup(value);

	} else if (strcmp(name, "installed") == 0) {
		if (strcmp(value, "no") != 0) {
			rp->enforced ^= RES_PACKAGE_ABSENT;
		} else {
			rp->enforced |= RES_PACKAGE_ABSENT;
		}

	} else {
		return -1;
	}

	return 0;
}

int res_package_stat(void *res, const struct resource_env *env)
{
	struct res_package *rp = (struct res_package*)(res);

	assert(rp);
	assert(env);
	assert(env->package_manager);

	free(rp->installed);
	rp->installed = package_manager_query(env->package_manager, rp->name);

	return 0;
}

struct report* res_package_fixup(void *res, int dryrun, const struct resource_env *env)
{
	struct res_package *rp = (struct res_package*)(res);

	assert(rp);
	assert(env);
	assert(env->package_manager);

	struct report *report = report_new("Package", rp->name);
	char *action;

	if (res_package_enforced(rp, ABSENT)) {
		if (rp->installed) {
			action = string("uninstall package");

			if (dryrun) {
				report_action(report, action, ACTION_SKIPPED);
			} else if (package_manager_remove(env->package_manager, rp->name) == 0) {
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
		} else if (package_manager_install(env->package_manager, rp->name, rp->version) == 0) {
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
		} else if (package_manager_install(env->package_manager, rp->name, rp->version) == 0) {
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

char* res_package_pack(const void *res)
{
	const struct res_package *rp = (const struct res_package*)(res);
	assert(rp);

	return pack(RES_PACKAGE_PACK_PREFIX, RES_PACKAGE_PACK_FORMAT,
	            rp->enforced,
	            rp->name, rp->version);
}

void* res_package_unpack(const char *packed)
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

