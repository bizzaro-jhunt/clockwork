#!/bin/bash

task "Setting up files for userdb tests"

mkdir -p $TEST_UNIT_TEMP/userdb
mkdir -p $TEST_UNIT_DATA/userdb

echo " - creating user password file"
cat > $TEST_UNIT_DATA/userdb/passwd <<EOF
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/bin/sh
bin:x:2:2:bin:/bin:/bin/sh
sys:x:3:3:sys:/dev:/bin/sh
user:x:100:20:User Account,,,:/home/user/bin/bash
svc:x:999:909:service account:/tmp/nonexistent:/sbin/nologin
EOF

echo " - creating user shadow password file"
cat > $TEST_UNIT_DATA/userdb/shadow <<EOF
root:!:14009:0:99999:7:::
daemon:*:13991:0:99999:7:::
bin:*:13991:0:99999:7:::
sys:*:13991:0:99999:7:::
user:$6$nahablHe$1qen4PePmYtEIC6aCTYoQFLgMp//snQY7nDGU7.9iVzXrmmCYLDsOKc22J6MPRUuH/X4XJ7w.JaEXjofw9h1d/:14871:0:99999:7:::
svc:*:13991:0:99999:7:::
EOF

echo " - creating group password file"
cat > $TEST_UNIT_DATA/userdb/group <<EOF
root:x:0:
daemon:x:1:
bin:x:2:
sys:x:3:
members:x:4:account1,account2,account3
users:x:20:
service:x:909:account1,account2
EOF

echo " - creating group shadow password file"
cat > $TEST_UNIT_DATA/userdb/gshadow <<EOF
root:*::
daemon:*::
bin:*::
sys:*::
members:*:admin1,admin2:account1,account2,account3
users:*::
service:!:admin2:account1,account2
EOF

echo " - creating passwd-uid file"
cat > $TEST_UNIT_DATA/userdb/passwd-uid <<EOF
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/bin/sh
bin:x:2:2:bin:/bin:/bin/sh
sys:x:3:3:sys:/dev:/bin/sh
user:x:100:20:User Account,,,:/home/user/bin/bash
svc:x:999:909:service account:/tmp/nonexistent:/sbin/nologin
jrh1:x:2048:1000:UID 2048:/home/jrh1:/bin/bash
jrh2:x:2049:1000:UID 2049:/home/jrh2:/bin/bash
jrh3:x:42:1000:UID 42:/home/jrh3:/bin/bash
jrh4:x:8191:1000:UID 8191:/home/jrh4:/bin/bash
jrh5:x:800:1000:UID 800:/home/jrh5:/bin/bash
jrh6:x:1015:1000:UID 1015:/home/jrh6:/bin/bash
EOF
