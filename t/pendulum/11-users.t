#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use Test::Deep;

use Test::More;
use t::pendulum::common;

# NB: If this changes, all the tests will need to change as well...
my $TESTS = "t/data/pn/users";

# NB: If this changes, all the tests will need to change as well...
my $TEMP  = "t/tmp/pn/users";
qx(rm -rf $TEMP; mkdir -p $TEMP);

open PASSWD, ">", "$TEMP/passwd";
print PASSWD <<'EOF';
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/bin/sh
bin:x:2:2:bin:/bin:/bin/sh
sys:x:3:3:sys:/dev:/bin/sh
user:x:100:20:User Account,,,:/home/user/bin/bash
svc:x:999:909:service account:/tmp/nonexistent:/sbin/nologin
EOF
close PASSWD;

open SHADOW, ">", "$TEMP/shadow";
print SHADOW <<'EOF';
root:!:14009:0:99999:7:::
daemon:*:13991:0:99999:7:::
bin:*:13991:0:99999:7:::
sys:*:13991:0:99999:7:::
user:$6$nahablHe$1qen4PePmYtEIC6aCTYoQFLgMp//snQY7nDGU7.9iVzXrmmCYLDsOKc22J6MPRUuH/X4XJ7w.JaEXjofw9h1d/:14871:0:99999:7:::
svc:*:13991:0:99999:7:::
EOF

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

pendulum_ok "$TESTS/update.pn", <<'EOF', "update.pn";
username: svc2
uid: 1999
gid: 1909
passwd: lol
gecos: 'A New Service Account'
home: /var/lib/nowhere
shell: /bin/false
EOF

pendulum_ok "$TESTS/remove.pn", <<'EOF', "remove.pn";
found user 'user'
user not found, post-delete
EOF

pendulum_ok "$TESTS/create.pn", <<'EOF', "create.pn";
user 'new' does not exist
user 'new' found, post-create
UID is 9001
GID is 9002
EOF

pendulum_ok "$TESTS/nextuid.pn", <<'EOF', "nextuid.pn";
next UID >= 0 is 4
next UID >= 2 is 4
next UID >= 500 is 500
next UID >= 6500 is 6500
next UID >= 9001 is 9002
EOF

done_testing;
