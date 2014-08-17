#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

my $TESTS = "t/tmp/data/security";

my ($stdout, $stderr);
run_ok "./TEST_cogd -Fvc $TESTS/invalid/cogd.conf", 1, \$stdout, \$stderr,
	"Run cogd with an invalid certificate";

$stderr =~ s/^cogd\[\d+\]\s*//mg;
string_is $stderr, <<EOF, "invalid: Standard error output";
t/tmp/data/security/invalid/badcert: Invalid Clockwork certificate
EOF

string_is $stdout, '', "invalid: No standard output";



run_ok "./TEST_cogd -Fvc $TESTS/nocert/cogd.conf", 1, \$stdout, \$stderr,
	"Run cogd with no certificate";

$stderr =~ s/^cogd\[\d+\]\s*//mg;
string_is $stderr, <<EOF, "nocert: Standard error output";
/path/to/nowhere: No such file or directory
EOF

string_is $stdout, '', "nocert: No standard output";



run_ok "./TEST_cogd -Fvc $TESTS/pubonly/cogd.conf", 1, \$stdout, \$stderr,
	"Run cogd with pub-only certificate";

$stderr =~ s/^cogd\[\d+\]\s*//mg;
string_is $stderr, <<EOF, "pubonly: Standard error output";
t/tmp/data/security/pubonly/cert: No secret key found in certificate
EOF

string_is $stdout, '', "pubonly: No standard output";



run_ok "./TEST_cogd -Fvc $TESTS/noident/cogd.conf", 1, \$stdout, \$stderr,
	"Run cogd with anonymous certificate";

$stderr =~ s/^cogd\[\d+\]\s*//mg;
string_is $stderr, <<EOF, "noident: Standard error output";
t/tmp/data/security/noident/cert: No identity found in certificate
EOF

string_is $stdout, '', "noident: No standard output";



run_ok "./TEST_cogd -Fvc $TESTS/nomastercert/cogd.conf", 2, \$stdout, \$stderr,
	"Run cogd with no configured master certificates";

$stderr =~ s/^cogd\[\d+\]\s*//mg;
string_is $stderr, <<EOF, "nomastercert: Standard error output";
master.1 (127.0.0.1:3333) has no matching certificate (cert.1)
EOF

string_is $stdout, '', "nomastercert: No standard output";



run_ok "./TEST_cogd -Fvc $TESTS/badmastercert/cogd.conf", 2, \$stdout, \$stderr,
	"Run cogd with malformed master certificates";

$stderr =~ s/^cogd\[\d+\]\s*//mg;
string_is $stderr, <<EOF, "badmastercert: Standard error output";
cert.1 t/tmp/data/security/badmastercert/master.pub: Invalid Clockwork certificate
EOF

string_is $stdout, '', "badmastercert: No standard output";

done_testing;
