#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

command_ok qq(show version), <<EOF;
SHOW version
EOF

command_ok qq(show acls), <<EOF;
SHOW acls
EOF

command_ok qq(show acls for user1), <<EOF;
SHOW acls user1
EOF

command_ok qq(show acls for %group), <<EOF;
SHOW acls :group
EOF

done_testing;
