#!/bin/bash

task "Setting up directories for RESOURCE unit tests"

mkdir -p $TEST_UNIT_DATA/resource
mkdir -p $TEST_UNIT_TEMP/resource

D=$TEST_UNIT_DATA/resource/dir
rm -rf $D
mkdir -p $D
chown $TEST_SAFE_UID:$TEST_SAFE_GID $D
chmod 0700 $D
