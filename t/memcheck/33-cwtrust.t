#!/bin/bash

WORKDIR=$(mktemp -d)
trap "rm -rf $WORKDIR" EXIT TERM INT
./cw-cert -i test.host -f $WORKDIR/test

SCRIPTPATH=$(cd $(dirname $0); pwd -P);
$SCRIPTPATH/verify ./cw-trust -d $WORKDIR/trustdb $WORKDIR/test.pub
