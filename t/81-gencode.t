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
	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "/path/to/delete"
  call res.file.absent

fn main
  ;; file:/path/to/delete
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "/chmod-me"
  call res.file.present
  set %b 0644
  call res.file.chmod

fn main
  ;; file:/chmod-me
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "/home/user/stuff"
  call res.file.present
  set %b "user"
  call res.file.chown
  set %b "staff"
  call res.file.chgrp

fn main
  ;; file:/home/user/stuff
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "/etc/file.conf"
  call res.file.present
  set %b 0644
  call res.file.chmod
  set %b "/etc/file.conf"
  set %c "file:/etc/file.conf"
  call res.file.contents

fn main
  ;; file:/etc/file.conf
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "/etc/sudoers"
  call res.file.present
  set %b "/etc/.sudoers.cogd"
  set %d "visudo -vf /etc/.sudoers.cogd"
  set %e 0
  set %c "file:/etc/sudoers"
  call res.file.contents

fn main
  ;; file:/etc/sudoers
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "/etc/somefile"
  call res.file.present
  set %b "/etc/.xyz"
  set %d "/usr/local/bin/check-stuff"
  set %e 22
  set %c "file:/etc/somefile"
  call res.file.contents

fn main
  ;; file:/etc/somefile
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  call util.authdb.open
  set %a "t1user"
  call res.user.absent
  call util.authdb.save

fn main
  ;; user:t1user
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

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
    bail
  call util.authdb.save

fn main
  ;; user:t2user
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

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
    bail
  flagged? mkhome
  jnz +2
    user.get "home" %b
    call res.user.mkhome
  call util.authdb.save

fn main
  ;; user:t3user
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

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
    bail
  ;;; comment
  set %b "Name,,,,"
  user.set "comment" %b
  jz +2
    error "Failed to set %[a]s' GECOS comment to %[b]s"
    bail
  ;;; login shell
  set %b "/bin/bash"
  user.set "shell" %b
  jz +2
    error "Failed to set %[a]s' login shell to %[b]s"
    bail
  ;;; minimum password age
  set %b "99"
  user.set "pwmin" %b
  jz +2
    error "Failed to set %[a]s' minimum password age to %[b]li"
    bail
  ;;; maximum password age
  set %b "305"
  user.set "pwmax" %b
  jz +2
    error "Failed to set %[a]s' maximum password age to %[b]li"
    bail
  ;;; password warning period
  set %b "14"
  user.set "pwwarn" %b
  jz +2
    error "Failed to set %[a]s' password warning period to %[b]li"
    bail
  ;;; password inactivity period
  set %b "9998"
  user.set "inact" %b
  jz +2
    error "Failed to set %[a]s' password inactivity period to %[b]li"
    bail
  ;;; account expiration
  set %b "/bin/bash"
  user.set "expiry" %b
  jz +2
    error "Failed to set %[a]s' account expiration to %[b]li"
    bail
  call util.authdb.save

fn main
  ;; user:t4user
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  call util.authdb.open
  set %a "group1"
  call res.group.absent
  call util.authdb.save

fn main
  ;; group:group1
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  call util.authdb.open
  set %a "group2"
  set %b 6766
  call res.group.present
  call util.authdb.save

fn main
  ;; group:group2
  try res:00000001
EOF
		"group with specific gid");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	group "group3" { }
}
host "example" { enforce "example" }
EOF

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  call util.authdb.open
  set %a "group3"
  set %b 0
  call res.group.present
  call util.authdb.save

fn main
  ;; group:group3
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

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
  ;; group:group4
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

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
  ;; dir:/etc/sudoers
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "/path/to/delete"
  call res.dir.absent

fn main
  ;; dir:/path/to/delete
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "/chmod-me"
  call res.dir.present
  set %b 0755
  call res.file.chmod

fn main
  ;; dir:/chmod-me
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

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
  ;; dir:/home/user/bin
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "snmpd"
  call res.service.enable
  call res.service.start
  flagged? "service:snmpd"
  jz +1 ret
  call res.service.restart

fn main
  ;; service:snmpd
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "microcode"
  call res.service.stop

fn main
  ;; service:microcode
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "neverwhere"
  call res.service.disable
  call res.service.stop

fn main
  ;; service:neverwhere
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "snmpd"
  call res.service.enable
  call res.service.start
  flagged? "service:snmpd"
  jz +1 ret
  call res.service.reload

fn main
  ;; service:snmpd
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "binutils"
  set %b ""
  call res.package.install

fn main
  ;; package:binutils
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "binutils"
  call res.package.absent

fn main
  ;; package:binutils
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "binutils"
  set %b "1.2.3"
  call res.package.install

fn main
  ;; package:binutils
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "binutils"
  set %b "latest"
  call res.package.install

fn main
  ;; package:binutils
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "binutils"
  set %b ""
  call res.package.install

fn main
  ;; package:binutils
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

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
  ;; host:example.com
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %a "2.4.6.8"
  set %b "remove.me"
  call res.host.absent

fn main
  ;; host:remove.me
  try res:00000001
EOF
		"host removal");

	#######################################################
};
subtest "res_sysctl" => sub {
	mkdir "t/tmp";

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	sysctl "net.ipv6.icmp.ratelimit" {
		value: "1005"
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
  set %a "net/ipv6/icmp/ratelimit"
  set %b "net.ipv6.icmp.ratelimit"
  set %c "1005"
  set %d 0
  call res.sysctl.set

fn main
  ;; sysctl:net.ipv6.icmp.ratelimit
  try res:00000001
EOF
		"sysctl resource");

	#######################################################

	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	sysctl "net.ipv6.icmp.ratelimit" {
		value: "0"
		persist: "yes"
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
  set %a "net/ipv6/icmp/ratelimit"
  set %b "net.ipv6.icmp.ratelimit"
  set %c "0"
  set %d 1
  call res.sysctl.set

fn main
  ;; sysctl:net.ipv6.icmp.ratelimit
  try res:00000001
EOF
		"persistent sysctl value");

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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %b "/bin/ls -l /tmp"
  runas.uid 0
  runas.gid 0
  exec %b %d

fn main
  ;; exec:/bin/ls -l /tmp
  try res:00000001
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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

fn fix:00000001
  set %b "/bin/refresh-the-thing"
  runas.uid 0
  runas.gid 0
  flagged? "/bin/refresh-the-thing"
  jz +1 ret
  exec %b %d

fn main
  ;; exec:/bin/refresh-the-thing
  try res:00000001
EOF
		"ondemand exec resource");


	put_file "t/tmp/manifest.pol", <<'EOF';
policy "example" {
	exec "CONDITIONAL" {
		command: "/make-stuff"
		test:    "/usr/bin/test ! -f /stuff"
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
  set %b "/make-stuff"
  runas.uid 0
  runas.gid 0
  set %c "/usr/bin/test ! -f /stuff"
  exec %c %d
  jnz +1 ret
  exec %b %d

fn main
  ;; exec:CONDITIONAL
  try res:00000001
EOF
		"conditional exec resource");


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

	gencode2_ok(<<'EOF',
fn res:00000001
  unflag changed
  call fix:00000001
  ;; no dependencies

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
  ;; exec:catmans
  try res:00000001
EOF
		"runas user/group exec resource");

};

done_testing;
