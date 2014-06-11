#!/bin/bash
SCRIPTPATH=$(cd $(dirname $0); pwd -P);

mkdir -p t/tmp/pn/users/etc/
echo 'root:x:0:0:root:/root:/bin/bash' > t/tmp/pn/users/etc/passwd
echo 'root:!:14009:0:99999:7:::'       > t/tmp/pn/users/etc/shadow

$SCRIPTPATH/verify ./pn --nofork t/tmp/data/memcheck/pn/users.pn || exit $?
