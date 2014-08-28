#!/bin/bash
ETC=t/tmp/data/memcheck
SCRIPTPATH=$(cd $(dirname $0); pwd -P);

./TEST_cogd -c $ETC/cogd.conf -X &
$SCRIPTPATH/verify ./TEST_clockd -c $ETC/clockd.conf -F || exit $?
