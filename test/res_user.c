#include "test.h"
#include "../res_user.h"

#define ASSERT_ENFORCEMENT(o,f,c,t,v1,v2) do {\
	res_user_set_ ## f (o,v1); \
	assert_true( #c " enforced", res_user_enforced(o,c)); \
	assert_ ## t ## _equals( #c " set properly", (o)->ru_ ## f, v1); \
	\
	res_user_unset_ ## f (o); \
	assert_true( #c " no longer enforced", !res_user_enforced(o,c)); \
	\
	res_user_set_ ## f (o,v2); \
	assert_true( #c " re-enforced", res_user_enforced(o,c)); \
	assert_ ## t ## _equals ( #c " re-set properly", (o)->ru_ ## f, v2); \
} while(0)

void test_res_user_enforcement()
{
	struct res_user ru;
	res_user_init(&ru);

	test("RES_USER: Default Enforcements");
	assert_true("NAME not enforced", !res_user_enforced(&ru, NAME));
	assert_true("PASSWD not enforced", !res_user_enforced(&ru, PASSWD));
	assert_true("UID not enforced", !res_user_enforced(&ru, UID));
	assert_true("GID not enforced", !res_user_enforced(&ru, GID));

	test("RES_USER: NAME enforcement");
	ASSERT_ENFORCEMENT(&ru,name,NAME,str,"name1","name2");

	test("RES_USER: PASSWD enforcement");
	ASSERT_ENFORCEMENT(&ru,passwd,PASSWD,str,"pass1", "pass2");

	test("RES_USER: UID enforcement");
	ASSERT_ENFORCEMENT(&ru,uid,UID,int,4,8);

	test("RES_USER: GID enforcement");
	ASSERT_ENFORCEMENT(&ru,gid,GID,int,4,8);

	test("RES_USER: GECOS enforcement");
	ASSERT_ENFORCEMENT(&ru,gecos,GECOS,str,"Comment 1","Another GECOS");

	test("RES_USER: DIR enforcement");
	ASSERT_ENFORCEMENT(&ru,dir,DIR,str,"/home/user","/var/lib/subsys1");

	test("RES_USER: SHELL enforcement");
	ASSERT_ENFORCEMENT(&ru,shell,SHELL,str,"/bin/bash","/sbin/nologin");

	test("RES_USER: INACT enforcement");
	ASSERT_ENFORCEMENT(&ru,inact,INACT,int,45,999);

	test("RES_USER: EXPIRE enforcement");
	ASSERT_ENFORCEMENT(&ru,expire,EXPIRE,int,100,8989);

	test("RES_USER: LOCK enforcement");
	ASSERT_ENFORCEMENT(&ru,lock,LOCK,int,1,0);

	res_user_free(&ru);
}

void test_res_user_merge()
{
	struct res_user ru1, ru2;

	res_user_init(&ru1);
	ru1.ru_prio = 0;

	res_user_init(&ru2);
	ru2.ru_prio = 1;

	res_user_set_uid(&ru1, 123);
	res_user_set_gid(&ru1, 321);
	res_user_set_name(&ru2, "user");
	res_user_set_shell(&ru1, "/sbin/nologin");
	/*
	res_user_set_pwmin(&ru1, 2);
	res_user_set_pwmax(&ru1, 45);
	*/

	res_user_set_uid(&ru2, 999);
	res_user_set_gid(&ru2, 999);
	res_user_set_name(&ru2, "user");
	res_user_set_dir(&ru2, "/home/user");
	res_user_set_gecos(&ru2, "GECOS for user");
	/*
	res_user_set_pwmin(&ru2, 4);
	res_user_set_pwmax(&ru2, 90);
	res_user_set_pwwarn(&ru2, 14);
	*/
	res_user_set_inact(&ru2, 1000);
	res_user_set_expire(&ru2, 2000);

	test("RES_USER: Merging user resources together");
	res_user_merge(&ru1, &ru2);
	assert_int_equals("UID set properly after merge", ru1.ru_uid, 123);
	assert_int_equals("GID set properly after merge", ru1.ru_gid, 321);
	assert_str_equals("NAME set properly after merge", ru1.ru_name, "user");
	assert_str_equals("GECOS set properly after merge", ru1.ru_gecos, "GECOS for user");
	assert_str_equals("DIR set properly after merge", ru1.ru_dir, "/home/user");
	assert_str_equals("SHELL set properly after merge", ru1.ru_shell, "/sbin/nologin");
	/*
	assert_int_equals("PWMIN set properly after merge", ru1.ru_min, 2);
	assert_int_equals("PWMAX set properly after merge", ru1.ru_max, 45);
	assert_int_equals("PWWARN set properly after merge", ru1.ru_warn, 14);
	*/
	assert_int_equals("INACT set properly after merge", ru1.ru_inact, 1000);
	assert_int_equals("EXPIRE set properly after merge", ru1.ru_expire, 2000);
}

void test_suite_res_user()
{
	test_res_user_enforcement();
	test_res_user_merge();
}
