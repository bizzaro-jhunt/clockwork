#!/usr/bin/perl
print STDERR qx{ rm -f t/cover.pn.S*; ./pn --stdlib stdlib.pn -S t/cover.pn };
open STDERR, ">", "/dev/null";
exec './pn', '--cover', 't/cover.pn.S';
