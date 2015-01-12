#!/bin/bash
mkdir -p t/tmp
echo >t/tmp/manifest.pol <<'EOF'
policy "baseline" {
	file "/etc/sudoers" {
		owner:  "root"
		group:  "root"
		mode:    0400
		verify: "visudo -vf $tmpfile"
		source: "/cfm/files/sudoers"
	}
	file "/path/to/delete" {
		present: "no"
	}
}
policy "users" {
	user "user1" { present: "no" }
	user "user2" {
		uid: 1001
		gid: 1002
		home: "/home/user2"
		password: "$$cypto"
	}
}
fallback host {
	enforce "baseline"
	enforce "users"
}
EOF
SCRIPTPATH=$(cd $(dirname $0); pwd -P);
$SCRIPTPATH/verify ./cw-cc t/tmp/manifest.pol
