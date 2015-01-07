#!perl
use Test::More;
use t::common;
use POSIX qw/mkfifo/;
use IO::Socket::UNIX;

subtest "halt" => sub {
	pn2_ok(qq(
	fn main
		print "ok"
		halt
		print "fail"),

	"ok",
	"halt stops execution immediately");
};

subtest "comparison operators" => sub {
	pn2_ok(qq(
	fn main
		eq 0 0
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"eq 0 0");

	pn2_ok(qq(
	fn main
		eq 0 1
		jnz +1
		print "fail"
		print "ok"),

	"ok",
	"eq 0 1");

	pn2_ok(qq(
	fn main
		eq 1 0
		jnz +1
		print "fail"
		print "ok"),

	"ok",
	"eq 1 0");

	pn2_ok(qq(
	fn main
		set %a 42
		set %b 42
		eq %a %b
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"eq %a %b");

	pn2_ok(qq(
	fn main
		set %a 42
		eq %a 42
		jnz +2
			print "a == 42"
			ret
		print "a != 42"),

	"a == 42",
	"eq %a 42");

	pn2_ok(qq(
	fn main
		set %a 42
		eq 42 %a
		jnz +2
			print "42 == a"
			ret
		print "42 != a"),

	"42 == a",
	"eq 42 %a");

	pn2_ok(qq(
	fn main
		streq "foo" "foo"
		jnz +2
			print "foo == foo"
			ret
		print "foo != foo"),

	"foo == foo",
	'streq "foo" "foo"');

	pn2_ok(qq(
	fn main
		set %a "foo"
		set %b "foo"
		streq %a %b
		jnz +2
			print "a == b"
			ret
		print "a != b"),

	"a == b",
	'streq %a %b');

	pn2_ok(qq(
	fn main
		set %a "foo"
		streq %a "foo"
		jnz +2
			print "a == foo"
			ret
		print "a != foo"),

	"a == foo",
	'streq %a "foo"');

	pn2_ok(qq(
	fn main
		set %b "foo"
		streq "foo" %b
		jnz +2
			print "foo == b"
			ret
		print "foo != b"),

	"foo == b",
	'streq "foo" %b');

	pn2_ok(qq(
	fn main
		streq "foo" "FOO"
		jnz +2
			print "foo == FOO"
			ret
		print "foo != FOO"),

	"foo != FOO",
	'streq "foo" "FOO"');
};

subtest "jump operators" => sub {
	pn2_ok(qq(
	fn main
		jmp +1
		print "fail"
		print "ok"),

	"ok",
	"unconditional jump with an offset");

	pn2_ok(qq(
	fn main
		jmp over
		print "fail"
	over:
		print "ok"),

	"ok",
	"unconditional jump with a label");

	pn2_ok(qq(
	fn main
		eq 0 0
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"jz (jump-if-zero)");

	pn2_ok(qq(
	fn main
		eq 0 1
		jnz +1
		print "fail"
		print "ok"),

	"ok",
	"jnz (jump-if-not-zero)");

	pn2_ok(qq(
	fn main
		gt 42 0
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"gt 42 0");

	pn2_ok(qq(
	fn main
		gte 42 0
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"gte 42 0");

	pn2_ok(qq(
	fn main
		lt 0 42
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"lt 0 42");

	pn2_ok(qq(
	fn main
		lte 0 42
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"lte 0 42");
};

subtest "print operators" => sub {
	pn2_ok(qq(
	fn main
		print "hello, world"),

	"hello, world",
	"simple print statement, no format specifiers");

	pn2_ok(qq(
	fn main
		set %a 42
		set %b "bugs"

		print "found %[a]d %[b]s"),

	"found 42 bugs",
	"print statement with basic format specifiers");

	pn2_ok(qq(
	fn main
		print "110%%!"),

	"110%!",
	"literal '%' escape");

	pn2_ok(qq(
	fn main
		set %c "str"
		print "[%[c]8s]"),

	"[     str]",
	"width-modifiers in print format specifier");
};

subtest "string operator" => sub {
	pn2_ok(qq(
	fn main
		set %a 1
		set %b 2
		set %c 3
		string "easy as %[a]i-%[b]i-%[c]i" %d
		print "%[d]s"),

	"easy as 1-2-3",
	"string formatting");
};

subtest "register operators" => sub {
	pn2_ok(qq(
	fn main
		set %a 2
		push %a
		set %a 3
		pop %a
		print "a == %[a]d"),

	"a == 2",
	"push / pop");

	pn2_ok(qq(
	fn main
		set %a 1
		set %b 2
		swap %a %b
		print "a/b = %[a]d/%[b]d"),

	"a/b = 2/1",
	"swap");
};

subtest "math operators" => sub {
	pn2_ok(qq(
	fn main
		set  %a 1
		add  %a 4
		sub  %a 3
		mult %a 12
		div  %a 4
		print "%[a]d"),

	"6",
	"arithmetic");

	pn2_ok(qq(
	fn main
		set %a 17
		mod %a 10
		print "%[a]d"),

	"7",
	"modulo");
};

subtest "functions" => sub {
	pn2_ok(qq(
	fn main
		call print.a

	fn print.a
		print "a"
		call print.b
		call print.b
		print "a"

	fn print.b
		print "b"),

	"abba",
	"nested function calls");

	pn2_ok(qq(
	fn main
		call func

	fn func
		print "ok"
		ret
		print "fail"),

	"ok",
	"ret short-circuits function execution flow-control");

	pn2_ok(qq(
	fn main
		call func
		jnz +1
		print "fail"
		print "ok"
	fn func
		retv 3),

	"ok",
	"user-defined functions can return values");

	pn2_ok(qq(
	fn main
		call func
		acc %a
		print "func() == %[a]d"
	fn func
		retv 42),

	"func() == 42",
	"use acc opcode to get return value");
};

subtest "dump operator" => sub {
	my %opts = (
		postprocess => sub {
			my ($stdout) = @_;
			$stdout =~ s/\[[a-f0-9 ]+\] 0$/<program name> 0/m;
			return $stdout;
		},
	);

	pn2_ok(qq(
	fn main
		dump),

	qq(
    ---------------------------------------------------------------------
    %a [ 00000000 ]   %b [ 00000000 ]   %c [ 00000000 ]   %d [ 00000000 ]
    %e [ 00000000 ]   %f [ 00000000 ]   %g [ 00000000 ]   %h [ 00000000 ]
    %i [ 00000000 ]   %j [ 00000000 ]   %k [ 00000000 ]   %l [ 00000000 ]
    %m [ 00000000 ]   %n [ 00000000 ]   %o [ 00000000 ]   %p [ 00000000 ]

    acc: 00000000
     pc: 00000008

    data: | 80000000 | 0
          | 00000001 | 1
    inst: <s_empty>
    heap:
          <program name> 0
    ---------------------------------------------------------------------

),
	"dump a clean VM", %opts);

	pn2_ok(qq(
	fn main
		set %a 0x42
		set %p 0x24
		call func

	fn func
		set %a 0x36
		dump),

	qq(
    ---------------------------------------------------------------------
    %a [ 00000036 ]   %b [ 00000000 ]   %c [ 00000000 ]   %d [ 00000000 ]
    %e [ 00000000 ]   %f [ 00000000 ]   %g [ 00000000 ]   %h [ 00000000 ]
    %i [ 00000000 ]   %j [ 00000000 ]   %k [ 00000000 ]   %l [ 00000000 ]
    %m [ 00000000 ]   %n [ 00000000 ]   %o [ 00000000 ]   %p [ 00000024 ]

    acc: 00000000
     pc: 0000002e

    data: | 80000000 | 0
          | 00000001 | 1
          | 00000042 | 2
          | 00000000 | 3
          | 00000000 | 4
          | 00000000 | 5
          | 00000000 | 6
          | 00000000 | 7
          | 00000000 | 8
          | 00000000 | 9
          | 00000000 | 10
          | 00000000 | 11
          | 00000000 | 12
          | 00000000 | 13
          | 00000000 | 14
          | 00000000 | 15
          | 00000000 | 16
          | 00000024 | 17
    inst: | 00000020 | 0
    heap:
          <program name> 0
    ---------------------------------------------------------------------

),
	"dump a not-so-clean VM", %opts);
};

subtest "fs operators" => sub {
	mkdir "t/tmp";

	pn2_ok(qq(
	fn main
		fs.stat "t/tmp/enoent"
		jnz +1
		print "fail"
		print "ok"),

	"ok",
	"fs.stat for non-existent file");

	pn2_ok(qq(
	fn main
		fs.touch "t/tmp/newfile"
		jz +1
		print "touch-failed;"

		fs.stat "t/tmp/newfile"
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"fs.touch can create new files");

	pn2_ok(qq(
	fn main
		fs.unlink "t/tmp/newfile"
		jz +1
		print "FAIL"
		fs.stat "t/tmp/newfile"
		jnz +1
		print "fail"
		print "ok"),

	"ok",
	"fs.unlink can remove files");

	pn2_ok(qq(
	fn main
		set %a "t/tmp/oldname"
		fs.touch %a
		fs.stat %a
		jz +2
			perror "touch oldname"
			ret

		set %b "t/tmp/newname"
		fs.unlink %b
		fs.stat %b
		jnz +2
			error "unlink failed"
			ret

		fs.rename %a %b

		fs.stat %a
		jnz +2
			error "rename didnt remove oldname"
			ret

		fs.stat %b
		jz +2
			error "rename didnt create newname"
			ret

		print "ok"),

	"ok",
	"fs.rename renames files");

	pn2_ok(qq(
	fn main
		set %a "t/tmp/file"
		fs.unlink %a
		fs.touch %a
		fs.stat %a
		jz +2
		perror "stat failed"
		ret

		fs.inode %a %b
		gt %b 0
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"retrieved inode from file");

	SKIP: {
		skip "No /dev/null device found", 2
			unless -e "/dev/null";

		pn2_ok(qq(
		fn main
			fs.chardev? "/dev/null"
			jz +1
			print "fail"
			print "ok"),

		"ok",
		"/dev/null is a character device");

		pn2_ok(qq(
		fn main
			set %c "/dev/null"
			fs.major %c %a
			fs.minor %c %b
			print "%[a]d:%[b]d"),

		"1:3",
		"retrieved major/minor number for /dev/null");
	};

	SKIP: {
		pn2_ok(qq(
		fn main
			fs.blockdev? "/dev/loop0"
			jz +1
			print "fail"
			print "ok"),

		"ok",
		"/dev/loop0 is a block device");
	};

	pn2_ok(qq(
	fn main
		fs.dir? "t/tmp"
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"t/tmp is a directory");

	pn2_ok(qq(
	fn main
		fs.touch "t/tmp/file"
		fs.file? "t/tmp/file"
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"t/tmp/file is a file");

	symlink "t/tmp/file", "t/tmp/syml";
	pn2_ok(qq(
	fn main
		fs.symlink? "t/tmp/syml"
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"t/tmp/syml is a symbolic link");

	mkfifo "t/tmp/fifo", 0644;
	pn2_ok(qq(
	fn main
		fs.fifo? "t/tmp/fifo"
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"t/tmp/fifo is a FIFO pipe");

	IO::Socket::UNIX->new(Type => SOCK_STREAM, Local => "t/tmp/socket");
	pn2_ok(qq(
	fn main
		fs.socket? "t/tmp/socket"
		jz +1
		print "fail"
		print "ok"),

	"ok",
	"t/tmp/socket is a UNIX domain socket");

	SKIP: {
		skip "must be run as root for chown/chgrp tests", 2
			unless $< == 0;

		my ($uid, $gid) = ($> + 12, $) + 13);
		pn2_ok(qq(
		fn main
			set %a "t/tmp/chown"
			fs.touch %a

			fs.chgrp %a $gid
			fs.chown %a $uid

			fs.uid %a %b
			fs.gid %a %c
			print "%[b]d:%[c]d"),

		"$uid:$gid",
		"file ownership change / retrieval");
	};

	pn2_ok(qq(
	fn main
		set %a "t/tmp/chmod"
		fs.touch %a

		fs.chmod %a 0627
		fs.mode %a %b
		print "%[b]04o"),

	"0627",
	"chmod operation");

	SKIP: {
		skip "/etc/issue not found for stat tests", 1
			unless -e "/etc/issue";

		my @st = stat "/etc/issue";
		pn2_ok(qq(
		fn main
			set %p "/etc/issue"
			fs.dev   %p %a
			fs.inode %p %b
			fs.nlink %p %c
			fs.size  %p %d
			fs.atime %p %e
			fs.ctime %p %f
			fs.mtime %p %g
			fs.uid   %p %h
			fs.gid   %p %i
			print "dev=%[a]d\\n"
			print "ino=%[b]d\\n"
			print "n=%[c]d\\n"
			print "size=%[d]d\\n"
			print "atime=%[e]d\\n"
			print "ctime=%[f]d\\n"
			print "mtime=%[g]d\\n"
			print "uid=%[h]d\\n"
			print "gid=%[i]d\\n"),

		"dev=$st[0]\nino=$st[1]\nn=$st[3]\nsize=$st[7]\n".
		"atime=$st[8]\nctime=$st[10]\nmtime=$st[9]\nuid=$st[4]\ngid=$st[5]\n",
		"stat-based accessor opcodes");
	}
};

subtest "user management" => sub {
	mkdir "t/tmp/auth";
	put_file "t/tmp/auth/passwd", <<EOF;
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin
bin:x:2:2:bin:/bin:/usr/sbin/nologin
sys:x:3:3:sys:/dev:/usr/sbin/nologin
user1:x:1000:1100:Some User:/home/user1:/bin/bash
user2:x:1001:1101:Some User:/home/user2:/bin/bash
EOF
	put_file "t/tmp/auth/shadow", <<EOF;
root:HASH:15390:0:99999:7:::
daemon:*:15259:0:99999:7:::
bin:*:15259:0:99999:7:::
sys:*:15259:0:99999:7:::
user1:PWHASH:15259:0:99999:7:::
user2:PWHASH:15259:0:99999:7:::
EOF

	put_file "t/tmp/auth/group", <<EOF;
root:x:0:
daemon:x:1:
bin:x:2:
sys:x:3:
user1:x:1100:
user2:x:1101:user1,sys
EOF

	put_file "t/tmp/auth/gshadow", <<EOF;
root:*::
daemon:*::
bin:*::
sys:*::
user1:*::user2
user2:*::sys
EOF

	pn2_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		authdb.open
		jz +2
			perror "failed to open auth databases"
			halt

		authdb.close
		jz +2
			perror "failed to close auth databases"
			halt

		print "ok"),

	"ok",
	"open/close auth databases");

	pn2_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		authdb.open

		authdb.nextuid 1000 %a
		authdb.nextgid 1100 %b

		print "uid=%[a]d\\n"
		print "gid=%[b]d\\n"

		authdb.close),

	"uid=1002\ngid=1102\n",
	"nextuid / nextgid");

	pn2_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		authdb.open

		authdb.nextuid 0 %a
		authdb.nextgid 0 %b

		print "uid=%[a]d\\n"
		print "gid=%[b]d\\n"

		authdb.close),

	"uid=4\ngid=4\n",
	"nextuid / nextgid (starting at 0)");

	pn2_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		authdb.open

		user.find "user1"
		jz +2
			print "user 'user1' not found"
			ret

		user.get "bogus" %c
		jnz +1
			print "bogus attribute didnt fail as expected\\n"

		user.get "uid"      %a    print "uid=%[a]d\\n"
		user.get "gid"      %a    print "gid=%[a]d\\n"
		user.get "username" %a    print "username=%[a]s\\n"
		user.get "gecos"    %a    print "gecos=%[a]s\\n"
		user.get "home"     %a    print "home=%[a]s\\n"
		user.get "shell"    %a    print "shell=%[a]s\\n"
		user.get "password" %a    print "password=%[a]s\\n"
		user.get "pwhash"   %a    print "pwhash=%[a]s\\n"
		user.get "changed"  %a    print "changed=%[a]d\\n"
		user.get "pwmin"    %a    print "pwmin=%[a]d\\n"
		user.get "pwmax"    %a    print "pwmax=%[a]d\\n"
		user.get "pwwarn"   %a    print "pwwarn=%[a]d\\n"
		user.get "inact"    %a    print "inact=%[a]d\\n"
		user.get "expiry"   %a    print "expiry=%[a]d\\n"

		authdb.close
		print "ok\\n"
		ret),

	"uid=1000\n".
	"gid=1100\n".
	"username=user1\n".
	"gecos=Some User\n".
	"home=/home/user1\n".
	"shell=/bin/bash\n".
	"password=x\n".
	"pwhash=PWHASH\n".
	"changed=15259\n".
	"pwmin=0\n".
	"pwmax=99999\n".
	"pwwarn=7\n".
	"inact=-1\n".
	"expiry=-1\n".
	"ok\n",
	"user find / attribute retrieval");

	pn2_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		authdb.open

		user.find "user1"
		jz +2
			print "user 'user1' not found"
			ret

		user.set "bogus" 42
		jnz +1
			print "bogus attribute didnt fail as expected\\n"

		user.set "uid"        999
		user.set "gid"        999
		user.set "username"  "user99"
		user.set "gecos"     "Comment"
		user.set "home"      "/path/to/home"
		user.set "shell"     "/bin/false"
		user.set "password"  "<secret>"
		user.set "pwhash"    "decafbad"
		user.set "changed"    89818
		user.set "pwmin"      1010
		user.set "pwmax"      1212
		user.set "pwwarn"     4242
		user.set "inact"      12345
		user.set "expiry"     54321

		authdb.save
		authdb.open

		user.find "user99"
		jz +2
			print "user 'user99' not found"
			ret

		user.get "uid"      %a    print "uid=%[a]d\\n"
		user.get "gid"      %a    print "gid=%[a]d\\n"
		user.get "username" %a    print "username=%[a]s\\n"
		user.get "gecos"    %a    print "gecos=%[a]s\\n"
		user.get "home"     %a    print "home=%[a]s\\n"
		user.get "shell"    %a    print "shell=%[a]s\\n"
		user.get "password" %a    print "password=%[a]s\\n"
		user.get "pwhash"   %a    print "pwhash=%[a]s\\n"
		user.get "changed"  %a    print "changed=%[a]d\\n"
		user.get "pwmin"    %a    print "pwmin=%[a]d\\n"
		user.get "pwmax"    %a    print "pwmax=%[a]d\\n"
		user.get "pwwarn"   %a    print "pwwarn=%[a]d\\n"
		user.get "inact"    %a    print "inact=%[a]d\\n"
		user.get "expiry"   %a    print "expiry=%[a]d\\n"

		authdb.close
		print "ok\\n"
		ret),

	"uid=999\n".
	"gid=999\n".
	"username=user99\n".
	"gecos=Comment\n".
	"home=/path/to/home\n".
	"shell=/bin/false\n".
	"password=<secret>\n".
	"pwhash=decafbad\n".
	"changed=89818\n".
	"pwmin=1010\n".
	"pwmax=1212\n".
	"pwwarn=4242\n".
	"inact=12345\n".
	"expiry=54321\n".
	"ok\n",
	"user attribute update/retrieval");

	pn2_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		authdb.open

		group.find "sys"
		jz +2
			print "group 'sys' not found"
			ret

		group.get "bogus" %c
		jnz +1
			print "bogus attribute didnt fail as expected\\n"

		group.get "gid"      %a    print "gid=%[a]d\\n"
		group.get "name"     %a    print "name=%[a]s\\n"
		group.get "password" %a    print "password=%[a]s\\n"
		group.get "pwhash"   %a    print "pwhash=%[a]s\\n"

		print "ok\\n"),

	"gid=3\n".
	"name=sys\n".
	"password=x\n".
	"pwhash=*\n".
	"ok\n",
	"group find / attribute retreival");

	pn2_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		authdb.open

		group.find "sys"
		jz +2
			print "group 'sys' not found"
			ret

		group.set "bogus" 42
		jnz +1
			print "bogus attribute didnt fail as expected\\n"

		group.set "gid"       845
		group.set "name"     "systems"
		group.set "password" "SeCrEt!"
		group.set "pwhash"   "deadbeef"

		authdb.save
		authdb.open

		group.find "systems"
		jz +2
			print "group 'systems' not found"
			ret

		group.get "gid"      %a    print "gid=%[a]d\\n"
		group.get "name"     %a    print "name=%[a]s\\n"
		group.get "password" %a    print "password=%[a]s\\n"
		group.get "pwhash"   %a    print "pwhash=%[a]s\\n"

		print "ok\\n"),

	"gid=845\n".
	"name=systems\n".
	"password=SeCrEt!\n".
	"pwhash=deadbeef\n".
	"ok\n",
	"group find / attribute retreival");

	pn2_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		authdb.open

		user.find "bin"
		jz +2
			error "user 'bin' not found"
			ret

		user.delete
		authdb.save

		authdb.open
		user.find "bin"
		jnz +2
			error "user 'bin' not removed"
			ret

		print "ok"),

	"ok",
	"user.remove");

	pn2_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		authdb.open

		group.find "bin"
		jz +2
			error "group 'bin' not found"
			ret

		group.delete
		authdb.save

		authdb.open
		group.find "bin"
		jnz +2
			error "group 'bin' not removed"
			ret

		print "ok"),

	"ok",
	"group.remove");
};

done_testing;
