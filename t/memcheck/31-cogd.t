#!/bin/bash
ETC=t/tmp/data/memcheck
SCRIPTPATH=$(cd $(dirname $0); pwd -P);

./clockd -c $ETC/clockd.conf -F & 2>/dev/null
./cogd -c $ETC/cogd.conf -X
$SCRIPTPATH/verify ./cogd -c $ETC/cogd.conf -X || exit $?
