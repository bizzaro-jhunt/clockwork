#!/usr/bin/perl
use Test::More;
use t::common;

subtest "authdb" => sub {
	clean_tmp;
	mkdir "t/tmp/auth";
	put_file "t/tmp/auth/passwd", <<EOF;
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin
EOF
	put_file "t/tmp/auth/shadow", <<EOF;
root:HASH:15390:0:99999:7:::
daemon:*:15259:0:99999:7:::
EOF

	put_file "t/tmp/auth/group", <<EOF;
root:x:0:
daemon:x:1:abbadon
EOF

	put_file "t/tmp/auth/gshadow", <<EOF;
root:*::
daemon:*:abbadon:mephisto
EOF

	stdlib_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		call util.authdb.open
		user.find "daemon"
		jz +1 print "fail"
		print "ok\\n"
		call util.authdb.close),

	"ok\n",
	"util.authdb.open and .close work without errors");

	stdlib_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		call util.authdb.open
		user.find "daemon"
		jz +2
			print "daemon not found\\n"
			bail 1

		user.delete
		user.find "daemon"
		jnz +2
			print "daemon not removed\\n"
			bail 1

		call util.authdb.close
		call util.authdb.open
		user.find "daemon"
		jz +2
			print "util.authdb.close did not abandon changes\\n"
			bail 1

		print "ok\\n"),

	"ok\n",
	"util.authdb.close abandons changes");

	stdlib_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"
		call util.authdb.open
		user.find "daemon"
		jz +2
			print "daemon not found\\n"
			bail 1

		user.delete
		user.find "daemon"
		jnz +2
			print "daemon not removed\\n"
			bail 1

		call util.authdb.save
		call util.authdb.close
		call util.authdb.open
		user.find "daemon"
		jnz +2
			print "util.authdb.save just ... didn't\\n"
			bail 1

		print "ok\\n"),

	"ok\n",
	"util.authdb.save writes changes");

	# cleanup
	qx(rm -rf t/tmp/auth);
};

subtest "util.runuser / .rungroup" => sub {
SKIP: {
	skip "must be run as root for util.runuser / util.rungroup tests", 1
		unless $< == 0;
	stdlib_ok(qq(
	fn main
		call util.authdb.open
		set %a "bin" call util.runuser
		set %a "bin" call util.rungroup
		exec "/usr/bin/id" %c
		print "%[c]s\\n"),

	"uid=2(bin) gid=2(bin) groups=2(bin),0(root)\n",
	"runas bin:bin");
}};

subtest "res.file.absent" => sub {
	clean_tmp;
	stdlib_ok(qq(
	fn main
		set %a "t/tmp/enoent"

		;; setup
		fs.unlink %a
		fs.stat %a jnz +2
			print "enoent exists (and should not)"
			bail 1

		unflag "changed"
		call res.file.absent
		flagged? "changed" jnz +1 print "FLAGGED"
		fs.stat %a         jnz +1 print "stat failed"
		                          print "ok"),

	"ok",
	"res.file.absent can deal with absent files");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		set %a "t/tmp/file"

		fs.put %a "{contents}"
		fs.file? %a jz +2
			print "file does not exist (and should)"
			bail 1

		unflag "changed"
		call res.file.absent
		flagged? "changed" jz +1 print "!FLAGGED"
		fs.stat %a         jnz +1 print "fail"
		                          print "ok"),

	"ok",
	"res.file.absent can deal with files");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		set %a "t/tmp/dir"

		fs.mkdir %a
		fs.dir? %a jz +2
			print "dir does not exist (and should)"
			bail 1

		unflag "changed"
		call res.file.absent
		flagged? "changed" jnz +1 print "FLAGGED"
		fs.stat %a         jz +1 print "stat fail"
		fs.dir? %a         jz +1 print "dir? fail"
		                          ret"),

	"t/tmp/dir already exists, as a directory, and will not be automatically removed\n",
	"res.file.absent skips directories");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		set %a "t/tmp/symlink"

		fs.symlink "target" %a jz +1
		fs.symlink? %a jz +2
			print "symlink does not exist (and should)"
			bail 1

		unflag "changed"
		call res.file.absent
		flagged? "changed" jz +1 print "!FLAGGED"
		fs.stat %a         jnz +1 print "stat fail"
		                          print "ok"),

	"ok",
	"res.file.absent can handle symlinks");
};

subtest "res.dir.absent" => sub {
	clean_tmp;
	stdlib_ok(qq(
	fn main
		set %a "t/tmp/enoent"

		fs.stat %a jnz +2
			print "enoent exists (and shoud not)"
			bail 1

		unflag "changed"
		call res.dir.absent
		flagged? "changed" jnz +1 print "FLAGGED"
		fs.stat %a         jnz +1 print "fail"
		                          print "ok"),

	"ok",
	"res.dir.absent can deal with absent files");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		set %a "t/tmp/dir"

		fs.mkdir %a
		fs.dir? %a jz +2
			print "dir does not exist (and should)"
			bail 1

		unflag "changed"
		call res.dir.absent
		flagged? "changed" jz +1 print "!FLAGGED"
		fs.stat %a         jnz +1 print "stat fail"
		                          print "ok"),

	"ok",
	"res.dir.absent can deal with directories");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		set %a "t/tmp/file"

		fs.put %a "{contents}"
		fs.file? %a jz +2
			print "file does not exist (and should)"
			bail 1

		unflag "changed"
		call res.dir.absent
		flagged? "changed" jnz +1 print "FLAGGED"
		fs.stat %a         jz +1 print "fail"
		fs.file? %a        jz +1 print "file? fail"
		                         ret),

	"t/tmp/file already exists, as a regular file, and will not be automatically removed\n",
	"res.dir.absent skips files");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		set %a "t/tmp/symlink"

		fs.symlink "target" %a
		fs.symlink? %a jz +2
			print "symlink does not exist (and should)"
			bail 1

		unflag "changed"
		call res.dir.absent
		flagged? "changed" jz +1 print "!FLAGGED"
		fs.stat %a         jnz +1 print "fail"
		                          print "ok"),

	"ok",
	"res.dir.absent can handle symlinks");
};

subtest "res.dir.present" => sub {
	clean_tmp;
	stdlib_ok(qq(
	fn main
		umask 0 %p
		set %a "t/tmp/new"

		fs.stat %a jnz +2
			print "dir exists (and should not)"
			bail 1

		unflag "changed"
		call res.dir.present
		flagged? "changed" jz +1 print "!FLAGGED"
		fs.stat %a         jz +1 print "stat fail"
		fs.dir? %a         jz +1 print "dir? fail"
		fs.mode %a %b
		eq %b 0777         jz +1 print "mode fail: %[b]04o != 0777"
		                         print "ok"),

	"ok",
	"res.dir.present can create a new directory");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		umask 0 %p
		set %a "t/tmp/dir"

		fs.mkdir %a
		fs.dir? %a jz +2
			print "dir does not exist (and should)"
			bail 1

		unflag "changed"
		fs.inode %a %c

		call res.dir.present
		flagged? "changed" jnz +1 print "FLAGGED"
		fs.stat %a         jz +1 print "stat fail"
		fs.dir? %a         jz +1 print "dir? fail"
		fs.inode %a %d
		eq %c %d           jz +1 print "inode changed!"
		                   print "ok"),

	"ok",
	"res.dir.present doesn't do anything for dirs that exist");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		umask 0 %p
		set %a "t/tmp/file"

		fs.put %a "{contents}"
		fs.file? %a jz +2
			print "file does not exist (and should)"
			bail 1

		unflag "changed"
		call res.dir.present
		flagged? "changed" jnz +1 print "FLAGGED"
		fs.stat %a         jz +1 print "stat fail"
		fs.file? %a        jz +1 print "file? fail"
		                   ret),

	"t/tmp/file already exists, as a regular file, and will not be automatically removed\n",
	"res.dir.present can't autoremove a file that is in the way");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		umask 0 %p
		set %a "t/tmp/symlink"

		fs.symlink "target" %a
		fs.symlink? %a jz +2
			print "symlink does not exist (and should)"
			bail 1

		unflag "changed"
		call res.dir.present
		flagged? "changed" jz +1 print "FLAGGED"
		fs.stat %a         jz +1 print "stat fail"
		fs.dir? %a         jz +1 print "dir? fail"
		                   print "ok"),

	"ok",
	"res.dir.present can autoremove a symbolic link that is in the way");
};

subtest "res.file.present" => sub {
	clean_tmp;
	stdlib_ok(qq(
	fn main
		umask 0 %p
		set %a "t/tmp/new"

		fs.stat %a jnz +2
			print "new file already exists (and should not)"
			bail 1

		unflag "changed"
		set %a "t/tmp/new"
		call res.file.present
		flagged? "changed"  jz +1 print "!FLAGGED"
		fs.stat %a          jz +1 print "stat fail"
		fs.file? %a         jz +1 print "file? fail"

		fs.mode %a %b
		eq %b 0666          jz +1 print "mode fail: %[b]04o != 0666"
		                          print "ok"),

	"ok",
	"res.file.present can create a new file");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		umask 0 %p
		set %a "t/tmp/file"

		fs.put %a "{contents}"
		fs.chmod %a 0641
		fs.inode %a %b
		fs.file? %a jz +2
			print "file does not exist (and should)"
			bail 1

		unflag "changed"
		call res.file.present
		flagged? "changed" jnz +1 print "FLAGGED"
		fs.stat %a         jz +1 print "stat fail"
		fs.file? %a        jz +1 print "file? fail"

		fs.mode %a %c
		eq %c 0641         jz +1 print "mode fail: %[a]04o != 0641"

		fs.inode %a %c
		eq %c %b           jz +1 print "inode changed from %[b]i to %[c]i"
		                         print "ok"),

	"ok",
	"res.file.present doesn't do anything for files that exist");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		umask 0 %p
		set %a "t/tmp/dir"

		fs.mkdir %a
		fs.dir? %a jz +2
			print "dir does not exist (and should)"
			bail 1

		unflag "changed"
		call res.file.present
		flagged? "changed" jnz +1 print "FLAGGED"
		fs.dir? %a         jz +1 print "dir? fail"
		                   ret),

	"t/tmp/dir already exists, as a directory, and will not be automatically removed\n",
	"res.file.present can't autoremove a dir that is in the way");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		umask 0 %p
		set %a "t/tmp/symlink"

		fs.symlink "target" %a
		fs.symlink? %a jz +2
			print "symlink does not exist (and should)"
			bail 1

		unflag "changed"
		call res.file.present
		flagged? "changed" jz +1 print "FLAGGED"
		fs.stat %a         jz +1 print "stat fail"
		fs.file? %a        jz +1 print "file? fail"
		                         print "ok"),

	"ok",
	"res.file.present can autoremove a symbolic link that is in the way");
};

subtest "res.file.chown" => sub {
SKIP: {
	skip "must be root for ownership tests", 1
		unless $> == 0;

	ok "tdb";
}};

subtest "res.file.chgrp" => sub {
SKIP: {
	skip "must be root for ownership tests", 1
		unless $> == 0;

	mkdir "t/tmp/auth";
	put_file "t/tmp/auth/passwd", <<EOF;
root:x:0:0:root:/root:/bin/bash
daemon:x:1:1:daemon:/usr/sbin:/usr/sbin/nologin
EOF
	put_file "t/tmp/auth/shadow", <<EOF;
root:HASH:15390:0:99999:7:::
daemon:*:15259:0:99999:7:::
EOF

	put_file "t/tmp/auth/group", <<EOF;
root:x:0:
daemon:x:1:abbadon
EOF

	put_file "t/tmp/auth/gshadow", <<EOF;
root:*::
daemon:*:abbadon:mephisto
EOF

	stdlib_ok(qq(
	fn main
		pragma authdb.root "t/tmp/auth"

		set %a "t/tmp/file"
		set %b "daemon"         ;; GID 1

		fs.put %a "{contents}"
		fs.gid %a %o
		eq %o 1
		jnz +2
			print "test file already owned by GID 1\\n"
			bail 1

		unflag "changed"
		call res.file.chgrp
		flagged? "changed" jz +1 print "!FLAGGED"
		fs.gid %a %p
		eq %p 1            jz +1 print "chgrp failed"
		                         print "ok"),

	"ok",
	"res.file.chgrp can change group ownership on a file");

	# FIXME: continue this
}};

subtest "res.file.chmod" => sub {
	clean_tmp;
	stdlib_ok(qq(
	fn main
		set %a "t/tmp/file"
		set %b 0641

		fs.put %a "{contents}"
		fs.mode %a %p
		eq %b %p jnz +2
			print "new file already has perms of 0641"
			bail 1

		unflag "changed"
		call res.file.chmod
		flagged? "changed" jz +1 print "!FLAGGED"
		fs.mode %a %p
		eq %p %b           jz +1 print "mode fail: %[p]04o != %[b]04o"
		                         print "ok"),

	"ok",
	"res.file.chmod can handle basic files that need changes");

	clean_tmp;
	stdlib_ok(qq(
	fn main
		umask 0 %p
		set %a "t/tmp/file"
		set %b 0666

		fs.put %a "{contents}"
		fs.mode %a %p
		eq %b %p jz +2
			print "new file has different perms (%[p]04o, not 0666)"
			bail 1

		unflag "changed"
		call res.file.chmod
		flagged? "changed" jnz +1 print "FLAGGED"
		fs.mode %a %p
		eq %p %b           jz +1 print "mode fail: %[p]04o != %[b]04o"
		                         print "ok"),

	"ok",
	"res.file.chmod doesnt flag if it doesnt need to");
};

subtest "res.file.contents" => sub {
	ok "tdb";
};

subtest "res.symlink.ensure" => sub {
	ok "tdb";
};

subtest "res.user.absent" => sub {
	ok "tdb";
};

subtest "res.user.present" => sub {
	ok "tdb";
};

subtest "res.group.absent" => sub {
	ok "tdb";
};

subtest "res.user.mkhome" => sub {
	ok "tdb";
};

subtest "util.copytree" => sub {
	ok "tdb";
};

subtest "res.group.present" => sub {
	ok "tdb";
};

subtest "res.package.absent" => sub {
	ok "tdb";
};

subtest "res.package.install" => sub {
	ok "tdb";
};

subtest "res.service.enable" => sub {
	ok "tdb";
};

subtest "res.service.disable" => sub {
	ok "tdb";
};

subtest "res.service.start" => sub {
	ok "tdb";
};

subtest "res.service.restart" => sub {
	ok "tdb";
};

subtest "res.service.reload" => sub {
	ok "tdb";
};

subtest "res.service.stop" => sub {
	ok "tdb";
};

subtest "res.host.absent" => sub {
	ok "tdb";
};

subtest "res.host.present" => sub {
	ok "tdb";
};

subtest "res.host.clear-aliases" => sub {
	ok "tdb";
};

subtest "res.host.add-alias" => sub {
	ok "tdb";
};

subtest "res.sysctl.set" => sub {
	ok "tdb";
};

done_testing;
