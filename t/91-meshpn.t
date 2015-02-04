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

command_ok qq(service ntpd start), <<EOF;
#include mesh
fn main
  set %a "ntpd"
  set %b "start"
  call mesh.service
EOF

command_ok qq(service whatever custom-action), <<EOF;
#include mesh
fn main
  set %a "whatever"
  set %b "custom-action"
  call mesh.service
EOF

command_ok qq(package useless-tools remove), <<EOF;
#include mesh
fn main
  set %a "useless-tools"
  set %b "remove"
  set %c ""
  call mesh.package
EOF

command_ok qq(package useless-tools install), <<EOF;
#include mesh
fn main
  set %a "useless-tools"
  set %b "install"
  set %c "latest"
  call mesh.package
EOF

command_ok qq(package useless-tools install 1.2.3), <<EOF;
#include mesh
fn main
  set %a "useless-tools"
  set %b "install"
  set %c "1.2.3"
  call mesh.package
EOF

command_ok qq(package recache), <<EOF;
#include mesh
fn main
  set %a ""
  set %b "recache"
  set %c ""
  call mesh.package
EOF

command_ok qq(some crazy command), <<EOF;
#include mesh
fn main
  set %a "some crazy command"
  call mesh.unhandled
EOF

done_testing;
