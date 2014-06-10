#!/bin/bash
export COGD_SKIP_ROOT=1

ETC=t/tmp/data/memcheck
SCRIPTPATH=$(cd $(dirname $0); pwd -P);

./cogd -c $ETC/cogd.conf -X &
$SCRIPTPATH/verify ./clockd -c $ETC/clockd.conf -F || exit $?
