#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use File::Slurp qw/write_file/;
use t::pendulum::common;

# NB: If these change, all the tests will need to change as well...
my $TESTS = "t/data/pn/users";
my $TEMP  = "t/tmp/pn/users";

my $PASSWD_DB = <<'EOF';
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/bin/sh
bin:x:2:2:bin:/bin:/bin/sh
sys:x:3:3:sys:/dev:/bin/sh
user:x:100:20:User Account,,,:/home/user:/bin/bash
svc:x:999:909:service account:/tmp/nonexistent:/sbin/nologin
EOF

my $SHADOW_DB = <<'EOF';
root:!:14009:0:99999:7:::
daemon:*:13991:0:99999:7:::
bin:*:13991:0:99999:7:::
sys:*:13991:0:99999:7:::
user:$6$nahablHe$1qen4PePmYtEIC6aCTYoQFLgMp//snQY7nDGU7.9iVzXrmmCYLDsOKc22J6MPRUuH/X4XJ7w.JaEXjofw9h1d/:14871:0:99999:7:::
svc:*:13991:0:99999:7:::
EOF

sub revert
{
	qx(rm -rf $TEMP; mkdir -p $TEMP);
	write_file "$TEMP/passwd", $PASSWD_DB;
	write_file "$TEMP/shadow", $SHADOW_DB;
}

sub no_change
{
	my ($test) = @_;
	local $Test::Builder::Level = $Test::Builder::Level + 1;
	file_is "$TEMP/passwd", $PASSWD_DB, "no changes to passwd database, after $test";
	file_is "$TEMP/shadow", $SHADOW_DB, "no changes to shadow database, after $test";
}

revert;
pendulum_ok "$TESTS/lookup.pn", <<'EOF', "lookup.pn";
passwd opened
shadow opened
found root user
root has UID/GID 0:0
found sys user
sys has UID/GID 3:3
found svc user
svc has UID/GID 999:909
found UID 100
UID 100 is 'user'
ballyhoo user not found
EOF
no_change "lookup.pn";

revert;
pendulum_ok "$TESTS/details.pn", <<'EOF', "details.pn";
username: svc
uid: 999
gid: 909
passwd: x
gecos: 'service account'
home: /tmp/nonexistent
shell: /sbin/nologin
pwhash: '*'
pwmin: 0
pwmax: 99999
pwwarn: 7
expire: -1
inact: -1
EOF
no_change "details.pn";

revert;
pendulum_ok "$TESTS/update.pn", <<'EOF', "update.pn";
username: svc2
uid: 1999
gid: 1909
passwd: lol
gecos: 'A New Service Account'
home: /var/lib/nowhere
shell: /bin/false
pwmin: 90
pwmax: 104
pwwarn: 14
inact: 12345
expire: 2468
EOF
file_is "$TEMP/passwd", <<'EOF', "passwd database updated with updates";
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/bin/sh
bin:x:2:2:bin:/bin:/bin/sh
sys:x:3:3:sys:/dev:/bin/sh
user:x:100:20:User Account,,,:/home/user:/bin/bash
svc2:lol:1999:1909:A New Service Account:/var/lib/nowhere:/bin/false
EOF
file_is "$TEMP/shadow", <<'EOF', "shadow database updated with updates";
root:!:14009:0:99999:7:::
daemon:*:13991:0:99999:7:::
bin:*:13991:0:99999:7:::
sys:*:13991:0:99999:7:::
user:$6$nahablHe$1qen4PePmYtEIC6aCTYoQFLgMp//snQY7nDGU7.9iVzXrmmCYLDsOKc22J6MPRUuH/X4XJ7w.JaEXjofw9h1d/:14871:0:99999:7:::
svc2:*:13991:90:104:14:12345:2468:
EOF

revert;
pendulum_ok "$TESTS/remove.pn", <<'EOF', "remove.pn";
found user 'user'
user not found, post-delete
EOF
file_is "$TEMP/passwd", <<'EOF', "passwd database updated with removals";
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/bin/sh
bin:x:2:2:bin:/bin:/bin/sh
sys:x:3:3:sys:/dev:/bin/sh
svc:x:999:909:service account:/tmp/nonexistent:/sbin/nologin
EOF
file_is "$TEMP/shadow", <<'EOF', "shadow database updated with removals";
root:!:14009:0:99999:7:::
daemon:*:13991:0:99999:7:::
bin:*:13991:0:99999:7:::
sys:*:13991:0:99999:7:::
svc:*:13991:0:99999:7:::
EOF

revert;
pendulum_ok "$TESTS/create.pn", <<'EOF', "create.pn";
user 'new' does not exist
user 'new' found, post-create
UID is 9001
GID is 9002
EOF
file_is "$TEMP/passwd", <<'EOF', "passwd database updated with additions";
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/bin/sh
bin:x:2:2:bin:/bin:/bin/sh
sys:x:3:3:sys:/dev:/bin/sh
user:x:100:20:User Account,,,:/home/user:/bin/bash
svc:x:999:909:service account:/tmp/nonexistent:/sbin/nologin
new::9001:9002:new:/:/bin/false
EOF
file_is "$TEMP/shadow", <<'EOF', "shadow database updated with additions";
root:!:14009:0:99999:7:::
daemon:*:13991:0:99999:7:::
bin:*:13991:0:99999:7:::
sys:*:13991:0:99999:7:::
user:$6$nahablHe$1qen4PePmYtEIC6aCTYoQFLgMp//snQY7nDGU7.9iVzXrmmCYLDsOKc22J6MPRUuH/X4XJ7w.JaEXjofw9h1d/:14871:0:99999:7:::
svc:*:13991:0:99999:7:::
new:*:0:0:99999:7:0:0:0
EOF

revert;
pendulum_ok "$TESTS/nextuid.pn", <<'EOF', "nextuid.pn";
next UID >= 0 is 4
next UID >= 2 is 4
next UID >= 500 is 500
next UID >= 999 is 1000
next UID >= 6500 is 6500
EOF
no_change "nextuid.pn";

done_testing;
