#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

my $TESTS = "t/tmp/data/security";
$ENV{TEST_CLOCKD_BAIL_EARLY} = 1;

my ($stdout, $stderr);
run_ok "./TEST_clockd -Fvc $TESTS/invalid/clockd.conf", 1, \$stdout, \$stderr,
	"Run clockd with an invalid certificate";

$stdout =~ s/^clockd\[\d+\]\s*//mg;
string_is $stdout, <<EOF, "invalid: Standard output";
t/tmp/data/security/invalid/badcert: Invalid Clockwork certificate
EOF

string_is $stderr, '', "invalid: No standard error output";



run_ok "./TEST_clockd -Fvc $TESTS/nocert/clockd.conf", 1, \$stdout, \$stderr,
	"Run clockd with an invalid certificate";

$stdout =~ s/^clockd\[\d+\]\s*//mg;
string_is $stdout, <<EOF, "nocert: Standard output";
/path/to/nowhere: No such file or directory
EOF

string_is $stderr, '', "nocert: No standard error output";



run_ok "./TEST_clockd -Fvc $TESTS/pubonly/clockd.conf", 1, \$stdout, \$stderr,
	"Run clockd with pub-only certificate";

$stdout =~ s/^clockd\[\d+\]\s*//mg;
string_is $stdout, <<EOF, "pubonly: Standard output";
t/tmp/data/security/pubonly/cert: No secret key found in certificate
EOF

string_is $stderr, '', "pubonly: No standard error output";



run_ok "./TEST_clockd -Fvc $TESTS/noident/clockd.conf", 1, \$stdout, \$stderr,
	"Run clockd with anonymous certificate";

$stdout =~ s/^clockd\[\d+\]\s*//mg;
string_is $stdout, <<EOF, "noident: Standard output";
t/tmp/data/security/noident/cert: No identity found in certificate
EOF

string_is $stderr, '', "noident: No standard error output";



done_testing;
