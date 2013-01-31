#!/bin/bash

task "Setting up directories for res_dir unit tests"

D=$TEST_UNIT_TEMP/res_dir/dir1
mkdir -p $D
if [[ $UID == 0 ]]; then
	chown $TEST_SAFE_UID:$TEST_SAFE_GID $D
	chmod 0755 $D
fi

D=$TEST_UNIT_TEMP/res_dir/fixme
mkdir -p $D
if [[ $UID == 0 ]]; then
	chown $TEST_SAFE_UID:$TEST_SAFE_GID $D
	chmod 0700 $D
fi
