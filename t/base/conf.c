#include <ctap.h>
#include <errno.h>
#include "../../src/conffile.h"
#include "../../src/gear/gear.h"

TESTS {
	struct hash *h = parse_config("t/data/config/good.conf");
	isnt_null(h, "parsed good.conf properly");
	is_string(hash_get(h, "count"), "2", "unquoted numeric literal");
	is_string(hash_get(h, "host"), "my.fq.dn", "unquoted string attr");
	is_string(hash_get(h, "leading"), "whitespace", "leading whitespace");
	is_string(hash_get(h, "inner"), "whitespace", "inner whitespace");
	is_string(hash_get(h, "lotsa"), "whitespace", "lotsa whitespace");
	is_string(hash_get(h, "final"), "FINAL", "no whitespace around '='");
	is_string(hash_get(h, "quoted1"), "a quoted string", "quoted strings");
	is_string(hash_get(h, "quoted2"), "another quote", "quoted with no space around '='");
	is_string(hash_get(h, "needless"), "no-spaces", "needless quotes honored");
	hash_free(h);

	done_testing();
}
