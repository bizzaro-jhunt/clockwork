#!/usr/bin/perl
use strict;
use warnings;
use Test::More;
use t::common;

subtest "res_file" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	file "/etc/sudoers" {
		owner: "root"
		group: "root"
		mode:  0400
	}
}
host "example" { enforce "example" }
EOF

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "/etc/sudoers"
  call res.file.present
  set %b "root"
  call res.file.chown
  set %b "root"
  call res.file.chgrp
  set %b 0400
  call res.file.chmod

fn main
  ;; file:/etc/sudoers
  try res:00000001
EOF
		"basic file resource");
};

subtest "res_user" => sub {
	mkdir "t/tmp";
	pass "tbd";
};
subtest "res_group" => sub {
	mkdir "t/tmp";
	pass "tbd";
};
subtest "res_dir" => sub {
	mkdir "t/tmp";
	pass "tbd";
};
subtest "res_service" => sub {
	mkdir "t/tmp";
	pass "tbd";
};
subtest "res_package" => sub {
	mkdir "t/tmp";
	pass "tbd";
};
subtest "res_host" => sub {
	mkdir "t/tmp";
	pass "tbd";
};
subtest "res_sysctl" => sub {
	mkdir "t/tmp";
	pass "tbd";
};
subtest "res_exec" => sub {
	mkdir "t/tmp";
	pass "tbd";
};

done_testing;
