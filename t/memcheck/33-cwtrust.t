#!/bin/bash

WORKDIR=$(mktemp -d)
trap "rm -rf $WORKDIR" EXIT TERM INT
./cwkey -i test.host -f $WORKDIR/test

SCRIPTPATH=$(cd $(dirname $0); pwd -P);
$SCRIPTPATH/verify ./cwtrust -d $WORKDIR/trustdb $WORKDIR/test.pub
