#!/usr/bin/perl
use strict;
use warnings;
use Test::More;
use t::common;

subtest "res_file" => sub {
	mkdir "t/tmp";

	#######################################################

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

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

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
  set %o 0
  topic "file:/etc/sudoers"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"file resource");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	file "/path/to/delete" {
		present: "no"
	}
}
host "example" { enforce "example" }
EOF
	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/path/to/delete"
  call res.file.absent

fn main
  set %o 0
  topic "file:/path/to/delete"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"file removal");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	file "/chmod-me" {
		present: "yes"
		mode:    0644
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/chmod-me"
  call res.file.present
  set %b 0644
  call res.file.chmod

fn main
  set %o 0
  topic "file:/chmod-me"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"file resource without chown/chgrp");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	file "/home/user/stuff" {
		present: "yes"
		owner:   "user"
		group:   "staff"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/home/user/stuff"
  call res.file.present
  set %b "user"
  call res.file.chown
  set %b "staff"
  call res.file.chgrp

fn main
  set %o 0
  topic "file:/home/user/stuff"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"file resource with non-root owner");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	file "/etc/file.conf" {
		present: "yes"
		mode:    0644
		source:  "/cfm/files/path/to/$path"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/etc/file.conf"
  call res.file.present
  set %b 0644
  call res.file.chmod
  set %b "/etc/file.conf"
  set %c "file:/etc/file.conf"
  call res.file.contents

fn main
  set %o 0
  topic "file:/etc/file.conf"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"file resource with content assertion");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	file "/etc/sudoers" {
		present: "yes"
		source:  "/cfm/files/sudoers"
		verify:  "visudo -vf $tmpfile"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/etc/sudoers"
  call res.file.present
  set %b "/etc/.sudoers.cogd"
  set %d "visudo -vf /etc/.sudoers.cogd"
  set %e 0
  set %c "file:/etc/sudoers"
  call res.file.contents

fn main
  set %o 0
  topic "file:/etc/sudoers"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"file resource with pre-change verification and default return code");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	file "/etc/somefile" {
		source:  "/cfm/files/something"
		tmpfile: "/etc/.xyz"
		verify:  "/usr/local/bin/check-stuff"
		expect:  22
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/etc/somefile"
  call res.file.present
  set %b "/etc/.xyz"
  set %d "/usr/local/bin/check-stuff"
  set %e 22
  set %c "file:/etc/somefile"
  call res.file.contents

fn main
  set %o 0
  topic "file:/etc/somefile"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"file resource with pre-change verification and explicit return code");

	#######################################################
};

subtest "res_user" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	user "t1user" {
		present: "no"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  call util.authdb.open
  set %a "t1user"
  call res.user.absent
  call util.authdb.save

fn main
  set %o 0
  topic "user:t1user"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"user removal");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	user "t2user" {
		uid:    1231
		gid:    1818
		home:   "/home/t2user"
		password: "$$crypto"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  call util.authdb.open
  set %a "t2user"
  set %b 1231 ; uid
  set %c 1818 ; gid
  set %d "/home/t2user" ; home
  set %e "" ; shell
  set %f "$$crypto" ; password
  call res.user.present
  ;;; home
  set %b "/home/t2user"
  user.set "home" %b
  jz +2
    error "Failed to set %[a]s' home directory to %[b]s"
    bail 1
  call util.authdb.save

fn main
  set %o 0
  topic "user:t2user"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"UID/GID, home and password management");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	user "t3user" {
		home:   "/home/t3user"
		password: "$$crypto"
		makehome: "yes"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  call util.authdb.open
  set %a "t3user"
  set %b 0 ; uid
  set %c 0 ; gid
  set %d "/home/t3user" ; home
  set %e "" ; shell
  set %f "$$crypto" ; password
  call res.user.present
  ;;; home
  set %b "/home/t3user"
  user.set "home" %b
  jz +2
    error "Failed to set %[a]s' home directory to %[b]s"
    bail 1
  flagged? "mkhome"
  jnz +2
    user.get "home" %b
    call res.user.mkhome
  call util.authdb.save

fn main
  set %o 0
  topic "user:t3user"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"home directory / password management");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	user "t4user" {
		uid:        1231
		gid:        1818
		home:       "/home/t4user"
		gecos:      "Name,,,,"
		shell:      "/bin/bash"
		pwmin:      99
		pwmax:      305
		pwwarn:     14
		inact:      9998
		expiration: 9999
		changepw:   "yes"
		password:   "$$crypto"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  call util.authdb.open
  set %a "t4user"
  set %b 1231 ; uid
  set %c 1818 ; gid
  set %d "/home/t4user" ; home
  set %e "/bin/bash" ; shell
  set %f "$$crypto" ; password
  call res.user.present
  ;;; home
  set %b "/home/t4user"
  user.set "home" %b
  jz +2
    error "Failed to set %[a]s' home directory to %[b]s"
    bail 1
  ;;; comment
  set %b "Name,,,,"
  user.set "comment" %b
  jz +2
    error "Failed to set %[a]s' GECOS comment to %[b]s"
    bail 1
  ;;; login shell
  set %b "/bin/bash"
  user.set "shell" %b
  jz +2
    error "Failed to set %[a]s' login shell to %[b]s"
    bail 1
  ;;; minimum password age
  set %b "99"
  user.set "pwmin" %b
  jz +2
    error "Failed to set %[a]s' minimum password age to %[b]li"
    bail 1
  ;;; maximum password age
  set %b "305"
  user.set "pwmax" %b
  jz +2
    error "Failed to set %[a]s' maximum password age to %[b]li"
    bail 1
  ;;; password warning period
  set %b "14"
  user.set "pwwarn" %b
  jz +2
    error "Failed to set %[a]s' password warning period to %[b]li"
    bail 1
  ;;; password inactivity period
  set %b "9998"
  user.set "inact" %b
  jz +2
    error "Failed to set %[a]s' password inactivity period to %[b]li"
    bail 1
  ;;; account expiration
  set %b "/bin/bash"
  user.set "expiry" %b
  jz +2
    error "Failed to set %[a]s' account expiration to %[b]li"
    bail 1
  call util.authdb.save

fn main
  set %o 0
  topic "user:t4user"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"all user attributes under the sun");

	#######################################################
};
subtest "res_group" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	group "group1" {
		present: "no"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  call util.authdb.open
  set %a "group1"
  call res.group.absent
  call util.authdb.save

fn main
  set %o 0
  topic "group:group1"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"group resource");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	group "group2" {
		gid: 6766
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  call util.authdb.open
  set %a "group2"
  set %b 6766
  call res.group.present
  call util.authdb.save

fn main
  set %o 0
  topic "group:group2"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"group with specific gid");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	group "group3" { }
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  call util.authdb.open
  set %a "group3"
  set %b 0
  call res.group.present
  call util.authdb.save

fn main
  set %o 0
  topic "group:group3"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"default group");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	group "group4" {
		gid:      6778
		password: "$$crypt"
		changepw: "yes"
		members:  "user1 !user3 user2"
		admins:   "!root adm1"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  call util.authdb.open
  set %a "group4"
  set %b 6778
  call res.group.present
  set %b "$$crypt"
  call res.group.passwd
  ;; FIXME: manage group membership!
  ;; FIXME: manage group adminhood!
  call util.authdb.save

fn main
  set %o 0
  topic "group:group4"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"group resource with member / admin changes");

	#######################################################
};
subtest "res_dir" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	dir "/etc/sudoers" {
		owner: "root"
		group: "root"
		mode:  0400
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/etc/sudoers"
  call res.dir.present
  set %b "root"
  call res.file.chown
  set %b "root"
  call res.file.chgrp
  set %b 0400
  call res.file.chmod

fn main
  set %o 0
  topic "dir:/etc/sudoers"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"dir resource");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	dir "/path/to/delete" {
		present: "no"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/path/to/delete"
  call res.dir.absent

fn main
  set %o 0
  topic "dir:/path/to/delete"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"dir removal");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	dir "/chmod-me" {
		present: "yes"
		mode:    0755
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/chmod-me"
  call res.dir.present
  set %b 0755
  call res.file.chmod

fn main
  set %o 0
  topic "dir:/chmod-me"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"dir resource with only a mode change");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	dir "/home/user/bin" {
		present: "yes"
		mode:    0710
		owner:   "user"
		group:   "staff"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "/home/user/bin"
  call res.dir.present
  set %b "user"
  call res.file.chown
  set %b "staff"
  call res.file.chgrp
  set %b 0710
  call res.file.chmod

fn main
  set %o 0
  topic "dir:/home/user/bin"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"dir resource with non-root owner / group");

	#######################################################
};
subtest "res_service" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	service "snmpd" {
		running: "yes"
		enabled: "yes"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "snmpd"
  call res.service.enable
  call res.service.start
  flagged? "service:snmpd"
  jz +1 ret
  call res.service.restart

fn main
  set %o 0
  topic "service:snmpd"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"service resource - running / enabled");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	service "microcode" {
		running: "no"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "microcode"
  call res.service.stop

fn main
  set %o 0
  topic "service:microcode"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"stopped service");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	service "neverwhere" {
		running:  "no"
		enabled:  "no"

		stopped:  "yes"
		disabled: "yes"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "neverwhere"
  call res.service.disable
  call res.service.stop

fn main
  set %o 0
  topic "service:neverwhere"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"stopped / disabled service (all keywords)");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	service "snmpd" {
		running: "yes"
		enabled: "yes"
		notify:  "reload"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "snmpd"
  call res.service.enable
  call res.service.start
  flagged? "service:snmpd"
  jz +1 ret
  call res.service.reload

fn main
  set %o 0
  topic "service:snmpd"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"service with reload notify mechanism");

	#######################################################
};
subtest "res_package" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	package "binutils" { }
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "binutils"
  set %b ""
  call res.package.install

fn main
  set %o 0
  topic "package:binutils"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"package resource");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	package "binutils" {
		installed: "no"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "binutils"
  call res.package.absent

fn main
  set %o 0
  topic "package:binutils"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"package removal");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	package "binutils" {
		installed: "yes"
		version: "1.2.3"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "binutils"
  set %b "1.2.3"
  call res.package.install

fn main
  set %o 0
  topic "package:binutils"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"explicit version specification");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	package "binutils" {
		installed: "yes"
		version: "latest"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "binutils"
  set %b "latest"
  call res.package.install

fn main
  set %o 0
  topic "package:binutils"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"'latest' version specification");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	package "binutils" {
		installed: "yes"
		version: "any"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "binutils"
  set %b ""
  call res.package.install

fn main
  set %o 0
  topic "package:binutils"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"'any' version will suffice");

	#######################################################
};
subtest "res_host" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	host "example.com" {
		ip: "1.2.3.4"
		alias: "www.example.com example.org"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "1.2.3.4"
  set %b "example.com"
  call res.host.present
  call res.host.clear-aliases
  set %c 0
  set %d "www.example.com"
  call res.host.add-alias
  set %d "example.org"
  call res.host.add-alias

fn main
  set %o 0
  topic "host:example.com"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"host resource with two aliases");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	host "remove.me" {
		present: "no"
		ip: "2.4.6.8"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %a "2.4.6.8"
  set %b "remove.me"
  call res.host.absent

fn main
  set %o 0
  topic "host:remove.me"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"host removal");

	#######################################################
};
subtest "res_exec" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	exec "/bin/ls -l /tmp" { }
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %b "/bin/ls -l /tmp"
  runas.uid 0
  runas.gid 0
  exec %b %d

fn main
  set %o 0
  topic "exec:/bin/ls -l /tmp"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"exec resource");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	exec "/bin/refresh-the-thing" {
		ondemand: "yes"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %b "/bin/refresh-the-thing"
  runas.uid 0
  runas.gid 0
  flagged? "/bin/refresh-the-thing"
  jz +1 ret
  exec %b %d

fn main
  set %o 0
  topic "exec:/bin/refresh-the-thing"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"ondemand exec resource");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	exec "CONDITIONAL" {
		command: "/make-stuff"
		test:    "/usr/bin/test ! -f /stuff"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %b "/make-stuff"
  runas.uid 0
  runas.gid 0
  set %c "/usr/bin/test ! -f /stuff"
  exec %c %d
  jnz +1 ret
  exec %b %d

fn main
  set %o 0
  topic "exec:CONDITIONAL"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"conditional exec resource");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	exec "catmans" {
		user:  "librarian"
		group: "booklovers"
		test:  "/bin/find /usr/share/mans -mtime +1 | grep -q 'xx'"
		ondemand: "no"
	}
}
host "example" { enforce "example" }
EOF

	gencode_ok(<<'EOF',
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000001
  set %b "catmans"
  runas.uid 0
  runas.gid 0
  call util.authdb.open
  set %a "librarian"
  call util.runuser
  set %a "booklovers"
  call util.rungroup
  call util.authdb.close
  set %c "/bin/find /usr/share/mans -mtime +1 | grep -q 'xx'"
  exec %c %d
  jnz +1 ret
  exec %b %d

fn main
  set %o 0
  topic "exec:catmans"
  try res:00000001
  acc %p
  add %o %p
  retv 0
EOF
		"runas user/group exec resource");

	#######################################################
};

subtest "if conditionals" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "00:always-there" { }

	if (sys.fqdn == "host.example.net") {
		package "01:double-equals" { }
	} else {
		package "01:double-equals-FAIL" { }
	}
	if (sys.fqdn is "host.example.net") {
		package "02:is" { }
	} else {
		package "02:is-FAIL" { }
	}

	if (sys.fqdn != "not-a-thing") {
		package "03:not-equals" { }
	} else {
		package "03:not-equals-FAIL" { }
	}

	if (sys.fqdn is not "not-a-thing") {
		package "04:is-not" { }
	} else {
		package "04:is-not-FAIL" { }
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'sys.fqdn' => 'host.example.net';

	resources_ok <<'EOF', "basic IF tests";
  package 00:always-there
  package 01:double-equals
  package 02:is
  package 03:not-equals
  package 04:is-not
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "00:always-there" { }

	if (sys.fqdn == "host.example.net" and sys.platform != "HPUX") {
		package "01:and" { }
	} else {
		package "01:and-FAIL" { }
	}
	if (sys.arch == "i386" or sys.arch == "x86_64") {
		package "02:or" { }
	} else {
		package "02:or-FAIL" { }
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'sys.platform' => "Linux",
		'sys.fqdn'     => 'host.example.net',
		'sys.arch'     => 'x86_64';

	resources_ok <<'EOF', "compound IF";
  package 00:always-there
  package 01:and
  package 02:or
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "00:always-there" { }

	if (sys.arch == lsb.arch) {
		package "01:fact-equality" { }
	} else {
		package "01:fact-equality-FAIL" { }
	}
	if (sys.arch != sys.kernel.verson) {
		package "02:fact-inequality" { }
	} else {
		package "02:fact-inequality-FAIL" { }
	}
	if ("test" == "test") {
		package "03:literal-to-literal" { }
	} else {
		package "03:literal-to-literal-FAIL" { }
	}

	if ("trusty" == lsb.distro.codename) {
		package "04:literal-to-fact" { }
	} else {
		package "04:literal-to-fact-FAIL" { }
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'sys.arch'            => 'x86_64',
		'lsb.arch'            => 'x86_64',
		'sys.kernel.version'  => '3.13.0-24-generic',
		'lsb.distro.codename' => 'trusty';

	resources_ok <<'EOF', "fact <=> fact";
  package 00:always-there
  package 01:fact-equality
  package 02:fact-inequality
  package 03:literal-to-literal
  package 04:literal-to-fact
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "00:always-there" { }

	if (lsb.distro.codename like m/lucid|trusty/) {
		package "01:like" { }
	} else {
		package "01:like-FAIL" { }
	}

	if (lsb.distro.codename !~ /lucid|trusty/) {
		package "03:unlike-FAIL" { }
	} else {
		package "03:unlike" { }
	}

	if (lsb.distro.codename =~ m|trusty|) {
		package "04:alt-delimiters" { }
	} else {
		package "04:alt-delimiters-FAIL" { }
	}

	# case-sensitive match with bad case...
	if (lsb.distro.codename not like m/TRUSTY|LUCID/) {
		package "05:case-sensitive" { }
	} else {
		package "05:case-sensitive-FAIL" { }
	}

	if (lsb.distro.codename like m/TRUSTY|LUCID/i) {
		package "06:case-insensitive" { }
	} else {
		package "06:case-insensitive-FAIL" { }
	}

	if (1 == 2) {
		package "07:multi-fail1" { }
	} else if (2 == 1) {
		package "07:multi-fail2" { }
	} else {
		package "07:multi"
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'lsb.distro.codename' => 'trusty';

	resources_ok <<'EOF', "regex conditionals";
  package 00:always-there
  package 01:like
  package 03:unlike
  package 04:alt-delimiters
  package 05:case-sensitive
  package 06:case-insensitive
  package 07:multi
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	package "00:always-there" { }

	if (sys.kernel.major =~ m/\d\.\d+$/) {
		package "01:slash-d" { }
	} else {
		package "01:slash-d-FAIL" { }
	}

	if (test.passwd =~ m/\/etc\/passwd/) {
		package "02:path-check" { }
	} else {
		package "02:path-check-FAIL" { }
	}

	if (test.passwd =~ m|/etc/passwd|) {
		package "03:alt-delim" { }
	} else {
		package "03:alt-delim-FAIL" { }
	}

	if (test.pipes =~ m/a\|b/) {
		package "04:pipes" { }
	} else {
		package "04:pipes-FAIL" { }
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'sys.kernel.major' => '3.13',
		'test.passwd'      => '/etc/passwd',
		'test.pipes'       => 'a|b|c|d|e';

	resources_ok <<'EOF', "advanced regexes";
  package 00:always-there
  package 01:slash-d
  package 02:path-check
  package 03:alt-delim
  package 04:pipes
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "host-a" { package "host-a" { } }
policy "host-b" { package "host-b" { } }
policy "host-c" { package "host-c" { } }
host "example" {
	enforce "host-a"
	if (lsb.distro.codename =~ m/trusty|lucid/) {
		enforce "host-b"
	}
	if (sys.arch is "sparc") {
		enforce "host-c" # not enforced
	}
}
EOF
	set_facts "t/tmp/facts.lst",
		'lsb.distro.codename' => 'trusty',
		'sys.arch'            => 'x86_64';

	resources_ok <<'EOF', "regex enforcement";
  package host-a
  package host-b
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "literals" {
		installed: "yes"
		version: map(sys.fqdn) {
			"host.example.net": "1.2.3"
			"other.host":       "1.FAIL"
			else:               "default.FAIL"
		}
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'sys.fqdn' => 'host.example.net';

	cw_shell_ok <<'EOF', "simple map conditional",

package "literals" {
  installed : "yes"
  name      : "literals"
  version   : "1.2.3"
}
EOF
	command => 'use host example; show package literals';

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "facts" {
		installed: "yes"
		version: map(sys.arch) {
			lsb.arch:           "correct"
			"x86_64":           "x86_64 fall-through"
			"i386":             "i386 fall-through"
			else:               "default fail"
		}
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'lsb.arch' => 'harvard',
		'sys.arch' => 'harvard';

	cw_shell_ok <<'EOF', "fact == fact map conditional",

package "facts" {
  installed : "yes"
  name      : "facts"
  version   : "correct"
}
EOF
	command => 'use host example; show package facts';

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "regex" {
		installed: "yes"
		version: map(sys.fqdn) {
			/example.com/:       "example.com"
			m/ietf.org/:         "ietf"
			/example.net/:       "correct"
			"host.example.net":  "exact match fall-through"
			default:             "default fail"
		}
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'sys.fqdn' => "host.example.net";

	cw_shell_ok <<'EOF', "regexes as rhs in map",

package "regex" {
  installed : "yes"
  name      : "regex"
  version   : "correct"
}
EOF
	command => "use host example; show package regex";

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "fallthrough" {
		installed: "yes"
		version: map(sys.arch) {
			"x86_64":           "x86_64 fail"
			"i386":             "i386 fail"
			else:               "default correct"
		}
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'sys.arch' => 'pa-risc';

	cw_shell_ok <<'EOF', "fact == value map conditional",

package "fallthrough" {
  installed : "yes"
  name      : "fallthrough"
  version   : "default correct"
}
EOF
	command => "use host example; show package fallthrough";

	#######################################################
};

subtest "dependencies" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	dir "/tmp" { }
	file "/tmp/inner/file" { }
	dir "/tmp/inner" { }
}
host "example" { enforce "example" }
EOF
	gencode_ok <<'EOF', "implict deps";
#include stdlib
fn res:00000001
  unflag "changed"
  call fix:00000001
  flagged? "changed"
  jz +1 retv 0
  flag "dir:/tmp"
  flag "dir:/tmp"
  retv 1

fn fix:00000001
  set %a "/tmp"
  call res.dir.present

fn res:00000003
  unflag "changed"
  call fix:00000003
  flagged? "changed"
  jz +1 retv 0
  flag "dir:/tmp/inner"
  retv 1

fn fix:00000003
  set %a "/tmp/inner"
  call res.dir.present

fn res:00000002
  unflag "changed"
  call fix:00000002
  flagged? "changed"
  jz +1 retv 0
  ;; no dependencies
  retv 1

fn fix:00000002
  set %a "/tmp/inner/file"
  call res.file.present

fn main
  set %o 0
  topic "dir:/tmp"
  try res:00000001
  acc %p
  add %o %p
  topic "dir:/tmp/inner"
  try res:00000003
  acc %p
  add %o %p
  topic "file:/tmp/inner/file"
  try res:00000002
  acc %p
  add %o %p
  retv 0
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "test" { }
	# dangling dependency
	package("test") depends on file("test.conf")
}
host "example" { enforce "example" }
EOF
	resources_ok <<'EOF', "dangling dependency target doesn't crash";
  package test
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "test" { }
	# dangling dependency
	package("test") affects file("test.conf")
}
host "example" { enforce "example" }
EOF
	resources_ok <<'EOF', "dangling dependency subject doesn't crash";
  package test
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "base" { }
	service "serviced" {
		running: "yes"
		enabled: "yes"

	}
	file "/etc/base.conf" {
		source: "/files/\$path"
	}

	service("serviced") depends on package("base")
	file("/etc/base.conf") depends on package("base")
	file("/etc/base.conf") affects service("serviced")
}
host "example" { enforce "example" }
EOF
	dependencies_ok <<'EOF', "explicit external depends on / affects";
  package base
     file /etc/base.conf
  service serviced
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "base" { }
	service "serviced" {
		running: "yes"
		enabled: "yes"

		depends on package("base")
	}
	file "/etc/base.conf" {
		source: "/files/\$path"

		depends on package("base")
		affects service("serviced")
	}
}
host "example" { enforce "example" }
EOF
	dependencies_ok <<'EOF', "explicit internal depends on / affects";
  package base
     file /etc/base.conf
  service serviced
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "A" { depends on package("B") }
	package "B" { depends on package("A") }
}
host "example" { enforce "example" }
EOF
	resources_ok <<'EOF', "circular dependency A -> B -> A doesn't crash";
  package A
  package B
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	package "A" { depends on package("B") }
	package "B" { depends on package("C") }
	package "C" { depends on package("D") }
	package "D" { depends on package("A") }
}
host "example" { enforce "example" }
EOF
	resources_ok <<'EOF', "circular dependency A -> B -> C -> D -> A doesn't crash";
  package A
  package B
  package C
  package D
EOF

	#######################################################
};

subtest "acl parsing" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	allow %systems "*"      final
	allow %dev     "show *"
	allow juser    "service restart *"
	deny  %probate "*"
}
host "example" { enforce "example" }
EOF

	acls_ok <<'EOF', "global ACLs";
  allow %systems "*" final
  allow %dev "show *"
  allow juser "service restart *"
  deny %probate "*"
EOF

	# FIXME: run gencode_ok to get ACL data
#include stdlib

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
	allow %systems "*" final

	if (sys.fqdn =~ m/example.net/) {
		allow %example "show *"
	}
	if (sys.platform == "HPUX") {
		allow %hpux "*"
	} else {
		allow %hpux "show *" final
		deny %hpux "*"
	}
}
host "example" { enforce "example" }
EOF
	set_facts "t/tmp/facts.lst",
		'sys.platform' => 'Linux',
		'sys.fqdn'     => 'host.example.net';

	acls_ok <<'EOF', "conditional ACLs";
  allow %systems "*" final
  allow %example "show *"
  allow %hpux "show *" final
  deny %hpux "*"
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example1" {
	allow %systems "*"      final
	allow %dev     "show *"
	allow juser    "service restart *"
	deny  %probate "*"
}
policy "example2" {
	allow %systems "*" final

	if (sys.fqdn =~ m/example.net/) {
		allow %example "show *"
	}
	if (sys.platform == "HPUX") {
		allow %hpux "*"
	} else {
		allow %hpux "show *" final
		deny %hpux "*"
	}
}
host "example" { enforce "example1" enforce "example2" }
EOF
	set_facts "t/tmp/facts.lst",
		'sys.platform' => "Linux",
		'sys.fqdn'     => "host.example.net";

	acls_ok <<'EOF', "multi-policy ACLs";
  allow %systems "*" final
  allow %dev "show *"
  allow juser "service restart *"
  deny %probate "*"
  allow %systems "*" final
  allow %example "show *"
  allow %hpux "show *" final
  deny %hpux "*"
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "baseline" {
  allow %base "query package *"
}
policy "example" {
  allow %pre "show version"
  extend "baseline"
  allow %level1 "exec netstat" final
  allow %systems "*"
}
host "example" { enforce "example" }
EOF
	acls_ok <<'EOF', "inherited ACLs";
  allow %pre "show version"
  allow %base "query package *"
  allow %level1 "exec netstat" final
  allow %systems "*"
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
  allow %group ALL
}
host "example" { enforce "example" }
EOF
	acls_ok <<'EOF', "ALL keyword";
  allow %group "*"
EOF

	#######################################################

	put_file "t/tmp/manifest.pol", <<EOF;
policy "example" {
  allow user ping
}
host "example" { enforce "example" }
EOF
	acls_ok <<'EOF', "unquoted strings";
  allow user "ping"
EOF

	#######################################################
};

subtest "unknown resource type" => sub {
	mkdir "t/tmp";

	put_file "t/tmp/manifest.pol", <<EOF;
policy "baseline" {
  dir  "dir" { }
  file "file" { }
  package "package" { }
  user "user" { }
  group "group" { }

  blah "unknown" { }
}
host fallback {
  enforce "baseline"
}
EOF

	put_file "t/tmp/unknown.conf", <<EOF;
security.cert t/tmp/cert.pub
manifest t/tmp/manifest.pol
EOF

	put_file "t/tmp/cert.pub", <<EOF;
id  test.box
pub 417b7f7946b6c65db58e86c5a66cbc698dbd1b15492e29372f927cf91620947e
sec 86ebac601b5b5b858886d223ab14dd9a1c0f8c0154fea416452b6afedca6db29
EOF
	note qx(./clockd -tvvvvvvvvv -c t/tmp/unknown.conf 2>&1);
	is $? >> 8, 2, "policy with unknown type 'blah' failed validation";
};

done_testing;
