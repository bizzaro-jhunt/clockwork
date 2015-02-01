#!/usr/bin/perl
$ENV{srcdir} = "$ENV{srcdir}/" if $ENV{srcdir};
$ENV{PENDULUM_INCLUDE} = "$ENV{srcdir}.";
print STDERR qx{ rm -f t/cover.pn.S*; ./pn -Sg $ENV{srcdir}t/cover.pn -o t/cover.pn.S };
open STDERR, ">", "/dev/null";
exec './TEST_pn', "t/cover.pn.S";
