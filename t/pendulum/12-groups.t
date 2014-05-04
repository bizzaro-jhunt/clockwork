#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use File::Slurp qw/write_file/;
use t::pendulum::common;

# NB: If these change, all the tests will need to change as well...
my $TESTS = "t/data/pn/groups";
my $TEMP  = "t/tmp/pn/groups";

my $GROUP_DB = <<EOF;
root:x:0:
daemon:x:1:
bin:x:2:
sys:x:3:
members:x:4:account1,account2,account3
users:x:20:
service:x:909:account1,account2
EOF

my $GSHADOW_DB = <<EOF;
root:*::
daemon:*::
bin:*::
sys:*::
members:*:admin1,admin2:account1,account2,account3
users:*::
service:!:admin2:account1,account2
EOF

sub revert
{
	qx(rm -rf $TEMP; mkdir -p $TEMP);
	write_file "$TEMP/group",   $GROUP_DB;
	write_file "$TEMP/gshadow", $GSHADOW_DB;
}

sub no_change
{
	my ($test) = @_;
	local $Test::Builder::Level = $Test::Builder::Level + 1;
	file_is "$TEMP/group",   $GROUP_DB,   "no changes to group database, after $test";
	file_is "$TEMP/gshadow", $GSHADOW_DB, "no changes to gshadow database, after $test";
}

revert;
pendulum_ok "$TESTS/lookup.pn", <<'EOF', "lookup.pn";
group opened
gshadow opened
found daemon group
group daemon has GID 1
found group members
group members has GID 4
found GID 909
GID 909 is 'service'
ballyhoo group not found
EOF
no_change "lookup.pn";


revert;
pendulum_ok "$TESTS/membership.pn", <<'EOF', "membership.pn";
checking group 'members'
admin1 is an admin
admin2 is an admin
admin3 is NOT an admin
account1 is a member
root is NOT a member

checking group 'bin'
root is NOT an admin
root is NOT a member

fin
EOF
no_change "membership.pn";

revert;
pendulum_ok "$TESTS/details.pn", <<'EOF', "details.pn";
name: service
gid: 909
passwd: 'x'
pwhash: '!'
EOF
no_change "details.pn";

revert;
pendulum_ok "$TESTS/update.pn", <<'EOF', "update.pn";
account1 is a member
admin1 is NOT an admin
name: renamed
gid: 919
passwd: '*'
pwhash: 'crypted.hash'
account1 is NOT a member
admin1 is an admin
EOF
file_is "$TEMP/group", <<'EOF', "group database updated with changes";
root:x:0:
daemon:x:1:
bin:x:2:
sys:x:3:
members:x:4:account1,account2,account3
users:x:20:
renamed:*:919:account2,account3
EOF
file_is "$TEMP/gshadow", <<'EOF', "gshadow database updated with changes";
root:*::
daemon:*::
bin:*::
sys:*::
members:*:admin1,admin2:account1,account2,account3
users:*::
renamed:crypted.hash:admin1:account2,account3
EOF

revert;
pendulum_ok "$TESTS/remove.pn", <<'EOF', "remove.pn";
found group 'users'
group not found, post-delete
EOF
file_is "$TEMP/group", <<'EOF', "group database updated with removals";
root:x:0:
daemon:x:1:
bin:x:2:
sys:x:3:
members:x:4:account1,account2,account3
service:x:909:account1,account2
EOF
file_is "$TEMP/gshadow", <<'EOF', "gshadow database updated with removals";
root:*::
daemon:*::
bin:*::
sys:*::
members:*:admin1,admin2:account1,account2,account3
service:!:admin2:account1,account2
EOF

revert;
pendulum_ok "$TESTS/create.pn", <<'EOF', "create.pn";
group 'new' does not exist
group 'new' found, post-create
GID is 9002
EOF
file_is "$TEMP/group", <<'EOF', "group database updated with additions";
root:x:0:
daemon:x:1:
bin:x:2:
sys:x:3:
members:x:4:account1,account2,account3
users:x:20:
service:x:909:account1,account2
new:x:9002:
EOF
file_is "$TEMP/gshadow", <<'EOF', "gshadow database updated with additions";
root:*::
daemon:*::
bin:*::
sys:*::
members:*:admin1,admin2:account1,account2,account3
users:*::
service:!:admin2:account1,account2
new:*::
EOF

revert;
pendulum_ok "$TESTS/nextgid.pn", <<'EOF', "nextgid.pn";
next GID >= 0 is 5
next GID >= 2 is 5
next GID >= 500 is 500
next GID >= 909 is 910
next GID >= 6500 is 6500
EOF
no_change "nextgid";

done_testing;
