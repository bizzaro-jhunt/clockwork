#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

my $CW_VERSION = "2.3.3";
my $CW_RUNTIME = "1";

my $TESTS = "t/tmp/data/pn/show";

pendulum_ok "$TESTS/version.pn", <<EOF, "version.pn";
$CW_VERSION
EOF

pendulum_ok "$TESTS/pendulum.pn", <<EOF, "pendulum.pn";
$CW_RUNTIME
EOF

pendulum_ok "$TESTS/multi.pn", <<EOF, "multi.pn";
$CW_VERSION
$CW_RUNTIME
$CW_VERSION
EOF

pendulum_ok "$TESTS/acl.pn", <<EOF, "acl.pn";
allow user1 "show version"
allow user2 "show *"
allow %systems "*" final
deny %dev "package *"
EOF

pendulum_ok "$TESTS/acl-user.pn", <<EOF, "acl-user.pn";
allow user1 "show version"
deny %dev "package *"
EOF

pendulum_ok "$TESTS/acl-group.pn", <<EOF, "acl-group.pn";
allow %systems "*" final
EOF

done_testing;
