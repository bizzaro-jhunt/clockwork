#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::pendulum::common;

my $TESTS = "t/data/pn/basics";

pendulum_ok "$TESTS/simple.pn", <<EOF, "simple.pn";
Hello, World!
EOF

pendulum_ok "$TESTS/halt.pn", <<EOF, "halt.pn";
Calling Halt...
EOF

pendulum_ok "$TESTS/implicit-halt.pn", <<EOF, "implicit-halt.pn";
Falling off the end of the program...
EOF

pendulum_ok "$TESTS/noop.pn", <<EOF, "noop.pn";
NOOPs are okay by me
EOF

pendulum_ok "$TESTS/jump.pn", <<EOF, "jump.pn";
\@start
\@previous
\@next
fin
EOF

pendulum_ok "$TESTS/ok.pn", <<EOF, "ok.pn";
OK? jumps if R register is non-zero
OK? continues if R register is zero
EOF

pendulum_ok "$TESTS/equality.pn", <<EOF, "equality.pn";
EQ? 42 54 was false
NE? 42 54 was true
GT? 42 54 was false
GTE? 42 54 was false
LT? 42 54 was true
LTE? 42 54 was true

EQ? 29 29 was true
NE? 29 29 was false
GT? 29 29 was false
GTE? 29 29 was true
LT? 29 29 was false
LTE? 29 29 was true

EQ? 1860 1776 was false
NE? 1860 1776 was true
GT? 1860 1776 was true
GTE? 1860 1776 was true
LT? 1860 1776 was false
LTE? 1860 1776 was false
fin
EOF

pendulum_ok "$TESTS/cmp.pn", <<EOF, "cmp.pn";
CMP? works
CMP? is case-sensitive
CMP? treats same strings as equal
fin
EOF

pendulum_ok "$TESTS/vcheck.pn", <<EOF, "vcheck.pn";
pendulum is at least v0
pendulum is not yet v9999
EOF

done_testing;
