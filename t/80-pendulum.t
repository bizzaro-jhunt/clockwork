#!perl
use Test::More;
use t::common;

subtest "comparison operators" => sub {
	pn2_ok(qq(
	fn main
		eq 0 0
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"eq 0 0");

	pn2_ok(qq(
	fn main
		eq 0 1
		jnz +1
		print "fail"
		print "ok"),

	"ok",
	"eq 0 1");

	pn2_ok(qq(
	fn main
		eq 1 0
		jnz +1
		print "fail"
		print "ok"),

	"ok",
	"eq 1 0");

	pn2_ok(qq(
	fn main
		set %a 42
		set %b 42
		eq %a %b
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"eq %a %b");

	pn2_ok(qq(
	fn main
		set %a 42
		eq %a 42
		jnz +2
			print "a == 42"
			ret
		print "a != 42"),

	"a == 42",
	"eq %a 42");

	pn2_ok(qq(
	fn main
		set %a 42
		eq 42 %a
		jnz +2
			print "42 == a"
			ret
		print "42 != a"),

	"42 == a",
	"eq 42 %a");

	pn2_ok(qq(
	fn main
		streq "foo" "foo"
		jnz +2
			print "foo == foo"
			ret
		print "foo != foo"),

	"foo == foo",
	'streq "foo" "foo"');

	pn2_ok(qq(
	fn main
		set %a "foo"
		set %b "foo"
		streq %a %b
		jnz +2
			print "a == b"
			ret
		print "a != b"),

	"a == b",
	'streq %a %b');

	pn2_ok(qq(
	fn main
		set %a "foo"
		streq %a "foo"
		jnz +2
			print "a == foo"
			ret
		print "a != foo"),

	"a == foo",
	'streq %a "foo"');

	pn2_ok(qq(
	fn main
		set %b "foo"
		streq "foo" %b
		jnz +2
			print "foo == b"
			ret
		print "foo != b"),

	"foo == b",
	'streq "foo" %b');

	pn2_ok(qq(
	fn main
		streq "foo" "FOO"
		jnz +2
			print "foo == FOO"
			ret
		print "foo != FOO"),

	"foo != FOO",
	'streq "foo" "FOO"');
};

subtest "jump operators" => sub {
	pn2_ok(qq(
	fn main
		jmp +1
		print "fail"
		print "ok"),

	"ok",
	"unconditional jump with an offset");

	pn2_ok(qq(
	fn main
		jmp over
		print "fail"
	over:
		print "ok"),

	"ok",
	"unconditional jump with a label");

	pn2_ok(qq(
	fn main
		eq 0 0
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"jz (jump-if-zero)");

	pn2_ok(qq(
	fn main
		eq 0 1
		jnz +1
		print "fail"
		print "ok"),

	"ok",
	"jnz (jump-if-not-zero)");

	pn2_ok(qq(
	fn main
		gt 42 0
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"gt 42 0");

	pn2_ok(qq(
	fn main
		gte 42 0
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"gte 42 0");

	pn2_ok(qq(
	fn main
		lt 0 42
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"lt 0 42");

	pn2_ok(qq(
	fn main
		lte 0 42
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"lte 0 42");
};

subtest "print operators" => sub {
	pn2_ok(qq(
	fn main
		print "hello, world"),

	"hello, world",
	"simple print statement, no format specifiers");

	pn2_ok(qq(
	fn main
		set %a 42
		set %b "bugs"

		print "found %[a]d %[b]s"),

	"found 42 bugs",
	"print statement with basic format specifiers");

	pn2_ok(qq(
	fn main
		print "110%%!"),

	"110%!",
	"literal '%' escape");

	pn2_ok(qq(
	fn main
		set %c "str"
		print "[%[c]8s]"),

	"[     str]",
	"width-modifiers in print format specifier");
};

subtest "math operators" => sub {
	pn2_ok(qq(
	fn main
		set  %a 1
		add  %a 4
		sub  %a 3
		mult %a 12
		div  %a 4
		print "%[a]d"),

	"6",
	"arithmetic");

	pn2_ok(qq(
	fn main
		set %a 17
		mod %a 10
		print "%[a]d"),

	"7",
	"modulo");
};

subtest "functions" => sub {
	pn2_ok(qq(
	fn main
		call print.a

	fn print.a
		print "a"
		call print.b
		call print.b
		print "a"

	fn print.b
		print "b"),

	"abba",
	"nested function calls");

	pn2_ok(qq(
	fn main
		call func

	fn func
		print "ok"
		ret
		print "fail"),

	"ok",
	"ret short-circuits function execution flow-control");
};

done_testing;
