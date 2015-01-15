#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

command_ok qq(show version), <<EOF;
fn main
  property "version" %a
  jz +2
    error "failed to retrieve the 'version' property"
    retv 1
  print "(v%[a]s)"
  retv 0
EOF

command_ok qq(show acls), <<EOF;
fn main
  show.acls
EOF

command_ok qq(show acls for user1), <<EOF;
fn main
  show.acl "user1"
EOF

command_ok qq(show acls for %group), <<EOF;
fn main
  show.acl ":group"
EOF

done_testing;
