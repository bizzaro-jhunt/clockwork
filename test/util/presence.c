#include <stdio.h>

#include "../../policy.h"
#include "../../spec/parser.h"

/** presence - a test utility

  presence reads a manifest file and then prints out
  all of the resources that are present and absent.
  The first line of the output contains user resources
  that are marked 'present', and the second line contains
  those that are absent.
 */

void print_users(struct policy *pol)
{
	struct res_user *u;
	size_t i;

	i = 0;
	printf("present");
	for_each_node(u, &pol->res_users, res) {
		if (!res_user_enforced(u, ABSENT)) {
			printf(":%s", u->ru_name);
			i++;
		}
	}
	if (i == 0) { printf(":none"); }
	printf("\n");

	i = 0;
	printf("absent");
	for_each_node(u, &pol->res_users, res) {
		if (res_user_enforced(u, ABSENT)) {
			printf(":%s", u->ru_name);
			i++;
		}
	}
	if (i == 0) { printf(":none"); }
	printf("\n");
}

int main(int argc, char **argv)
{
	struct manifest *manifest;
	struct policy *pol;

	if (argc != 2) {
		printf("improper invocation!\n");
		return 1;
	}

	manifest = parse_file(argv[1]);
	if (!manifest) {
		printf("unable to parse_file!\n");
		return 2;
	}

	pol = policy_generate(manifest->root->nodes[0], NULL);
	if (!pol) {
		printf("unable to policy_generate!\n");
		return 3;
	}

	print_users(pol);

	return 0;
}
