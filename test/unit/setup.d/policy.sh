#!/bin/bash

task "Creating policies for unit tests"

mkdir -p $TEST_UNIT_DATA/policy
mkdir -p $TEST_UNIT_TEMP/policy

mkdir -p $TEST_UNIT_DATA/policy/norm

F=$TEST_UNIT_DATA/policy/norm/policy.pol
cat > $F <<EOF
policy "base" {
	user "james" {
		uid: 1009
		gid: 2001
	}

	group "staff" {
		gid: 2001
	}

	file "test-file" {
		path:  "$TEST_UNIT_DATA/policy/norm/data/file"
		owner: "james"
		group: "staff"
	}

	dir "parent" {
		path:  "$TEST_UNIT_DATA/policy/norm/data"
		owner: "james"
		group: "staff"
		mode:  0750
	}

	dir "test-dir" {
		path:  "$TEST_UNIT_DATA/policy/norm/data/dir"
		owner: "james"
		group: "staff"
		mode:  0750
	}
}
EOF
