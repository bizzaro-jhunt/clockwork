#!/usr/bin/perl
$ENV{PENDULUM_INCLUDE} = ".";
print STDERR qx{ rm -f t/cover.pn.S*; ./pn -Sg t/cover.pn };
open STDERR, ">", "/dev/null";
exec './pn', '--cover', 't/cover.pn.S';
