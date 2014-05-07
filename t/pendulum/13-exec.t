#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::pendulum::common;

# NB: If this changes, all the tests will need to change as well...
my $TESTS = "t/data/pn/exec";

pendulum_ok "$TESTS/check.pn", <<'EOF', "check.pn";
pass exited 0
fail exited 1
crash exited 143
EOF

pendulum_ok "$TESTS/run1.pn", <<'EOF', "run1.pn";
pass said: "pass mode activated"
fail said: "exiting with RC=1"
crash said: ""
EOF


done_testing;
