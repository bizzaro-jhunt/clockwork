#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

my $TESTS = "t/tmp/data/security";
my ($stdout, $stderr, $pid, $test);

$test = "trust-strict-fail";
$pid = background_ok "./TEST_clockd -Fc $TESTS/$test/clockd.conf",
	"$test: run clockd in the background";
run_ok "./TEST_cogd -Xc $TESTS/$test/cogd.conf", 0, \$stdout, \$stderr,
	"$test: run cogd that isn't trusted by clockd in security.strict = yes";
$stderr =~ s/^cogd\[\d+\]\s*//mg;
string_is $stderr, <<EOF, "$test: Standard error output";
Starting configuration run
No response from master 1 (localhost:2313): possible certificate mismatch
No masters were reachable; giving up
EOF
kill 9, $pid;



$test = "trust-strict-ok";
$pid = background_ok "./TEST_clockd -Fc $TESTS/$test/clockd.conf",
	"$test: run clockd in the background";
run_ok "./TEST_cogd -Xc $TESTS/$test/cogd.conf", 0, \$stdout, \$stderr,
	"$test: run cogd that is trusted by clockd in security.strict = yes";
$stderr =~ s/^cogd\[\d+\]\s*//mg;
$stderr =~ s/=\d+/=X/g;
$stderr =~ s/in \d\.\d+s/in #.##s/;
string_is $stderr, <<EOF, "$test: Standard error output";
Starting configuration run
complete. enforced 0 resources in #.##s
STATS(ms): connect=X, hello=X, preinit=X, copydown=X, facts=X, getpolicy=X, parse=X, enforce=X, cleanup=X
EOF
kill 9, $pid;



$test = "trust-nostrict-fail";
$pid = background_ok "./TEST_clockd -Fc $TESTS/$test/clockd.conf",
	"$test: run clockd in the background";
run_ok "./TEST_cogd -Xc $TESTS/$test/cogd.conf", 0, \$stdout, \$stderr,
	"$test: run cogd that isn't trusted by clockd in security.strict = no";
$stderr =~ s/^cogd\[\d+\]\s*//mg;
string_is $stderr, <<EOF, "$test: Standard error output";
Starting configuration run
No response from master 1 (localhost:2313): possible certificate mismatch
No masters were reachable; giving up
EOF
kill 9, $pid;



$test = "trust-nostrict-ok";
$pid = background_ok "./TEST_clockd -Fc $TESTS/$test/clockd.conf",
	"$test: run clockd in the background";
run_ok "./TEST_cogd -Xc $TESTS/$test/cogd.conf", 0, \$stdout, \$stderr,
	"$test: run cogd that is trusted by clockd in security.strict = no";
$stderr =~ s/^cogd\[\d+\]\s*//mg;
$stderr =~ s/=\d+/=X/g;
$stderr =~ s/in \d\.\d+s/in #.##s/;
string_is $stderr, <<EOF, "$test: Standard error output";
Starting configuration run
complete. enforced 0 resources in #.##s
STATS(ms): connect=X, hello=X, preinit=X, copydown=X, facts=X, getpolicy=X, parse=X, enforce=X, cleanup=X
EOF
kill 9, $pid;



done_testing;
