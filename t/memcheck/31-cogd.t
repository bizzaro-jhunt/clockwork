#!/bin/bash
export COGD_SKIP_ROOT=1

ETC=t/tmp/data/memcheck
SCRIPTPATH=$(cd $(dirname $0); pwd -P);

./TEST_clockd -c $ETC/clockd.conf -F &
$SCRIPTPATH/verify ./TEST_cogd -c $ETC/cogd.conf -X || exit $?
