#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

command_ok qq(show version), <<EOF;
#include mesh
fn main
  call mesh.show.version
EOF

command_ok qq(show acls), <<EOF;
#include mesh
fn main
  call mesh.show.acls
EOF

command_ok qq(show acls for user1), <<EOF;
#include mesh
fn main
  set %a "user1"
  call mesh.show.acl
EOF

command_ok qq(show acls for %group), <<EOF;
#include mesh
fn main
  set %a ":group"
  call mesh.show.acl
EOF

command_ok qq(cfm), <<EOF;
#include mesh
fn main
  call mesh.cfm
EOF

command_ok qq(some crazy command), <<EOF;
#include mesh
fn main
  set %a "some crazy command"
  call mesh.unhandled
EOF


done_testing;
