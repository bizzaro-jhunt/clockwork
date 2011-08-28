#!/bin/bash

task "Setting up for RES_USER unit tests"

mkdir -p $TEST_UNIT_TEMP/res_user

rm -rf /tmp/nonexistent
rm -rf $TEST_UNIT_TEMP/new_user.home
