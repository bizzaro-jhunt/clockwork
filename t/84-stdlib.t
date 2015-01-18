#!/usr/bin/perl
$ENV{PENDULUM_INCLUDE} = ".";
print STDERR qx{ rm -f t/cover.pn.S*; ./pn -S t/cover.pn };
open STDERR, ">", "/dev/null";
exec './pn', '--cover', 't/cover.pn.S';
