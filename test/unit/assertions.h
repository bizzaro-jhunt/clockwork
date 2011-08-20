#ifndef _TEST_ASSERTIONS_H
#define _TEST_ASSERTIONS_H

#include <sys/types.h>
#include <stdarg.h>

#include "../../stringlist.h"
#include "../../policy.h"

void assert_stringlist(stringlist *sl, const char *name, size_t n, ...);
void assert_policy_has(const char *msg, const struct policy *pol, enum restype t, int num);

#define assert_policy_has_users(msg,p,n)  assert_policy_has(msg,p,RES_USER, n)
#define assert_policy_has_groups(msg,p,n) assert_policy_has(msg,p,RES_GROUP,n)
#define assert_policy_has_files(msg,p,n)  assert_policy_has(msg,p,RES_FILE, n)

void assert_dep(const struct policy *pol, const char *a, const char *b);

#endif
