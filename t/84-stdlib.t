#!/usr/bin/perl
use Test::More;
use t::common;

subtest "authdb" => sub {
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
	skip "must be run as root for util.runuser / util.rungroup tests", 2
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
	mkdir "t/tmp";
	mkdir "t/tmp/stdlib";
	put_file "t/tmp/stdlib/file", "file\n";
	symlink "target", "t/tmp/stdlib/symlink";
	mkdir "t/tmp/stdlib/dir";

	stdlib_ok(qq(
	fn main
		set %a "t/tmp/stdlib/enoent"
		call res.file.absent
		fs.stat %a
		jnz +1 print "fail"
		       print "ok"),

	"ok",
	"res.file.absent can deal with absent files");
	ok  -f "t/tmp/stdlib/file",    "'file' still exists";
	ok  -d "t/tmp/stdlib/dir",     "'dir' still exists";
	ok  -l "t/tmp/stdlib/symlink", "'symlink' still exists";

	stdlib_ok(qq(
	fn main
		set %a "t/tmp/stdlib/file"
		call res.file.absent
		fs.stat %a
		jnz +1 print "fail"
		       print "ok"),

	"ok",
	"res.file.absent can deal with files");
	ok !-f "t/tmp/stdlib/file", "'file' was removed";

	stdlib_ok(qq(
	fn main
		set %a "t/tmp/stdlib/dir"
		call res.file.absent
		fs.stat %a
		jnz +1 print "fail"
		       print "ok"),

	"t/tmp/stdlib/dir already exists, as a directory, and will not be automatically removed\n",
	"res.file.absent skips directories");
	ok -d "t/tmp/stdlib/dir", "'dir' still exists";

	stdlib_ok(qq(
	fn main
		set %a "t/tmp/stdlib/symlink"
		call res.file.absent
		fs.stat %a
		jnz +1 print "fail"
		       print "ok"),

	"ok",
	"res.file.absent can handle symlinks");
	ok !-l "t/tmp/stdlib/symlink", "'symlink' was removed";
};

subtest "res.dir.absent" => sub {
	mkdir "t/tmp";
	mkdir "t/tmp/stdlib";
	put_file "t/tmp/stdlib/file", "file\n";
	symlink "target", "t/tmp/stdlib/symlink";
	mkdir "t/tmp/stdlib/dir";

	stdlib_ok(qq(
	fn main
		set %a "t/tmp/stdlib/enoent"
		call res.dir.absent
		fs.stat %a
		jnz +1 print "fail"
		       print "ok"),

	"ok",
	"res.dir.absent can deal with absent files");
	ok -f "t/tmp/stdlib/file",    "'file' still exists";
	ok -d "t/tmp/stdlib/dir",     "'dir' still exists";
	ok -l "t/tmp/stdlib/symlink", "'symlink' still exists";

	stdlib_ok(qq(
	fn main
		set %a "t/tmp/stdlib/dir"
		call res.dir.absent
		fs.stat %a
		jnz +1 print "fail"
		       print "ok"),

	"ok",
	"res.dir.absent can deal with directories");
	ok !-d "t/tmp/stdlib/dir", "'dir' was removed";

	stdlib_ok(qq(
	fn main
		set %a "t/tmp/stdlib/file"
		call res.dir.absent
		fs.stat %a
		jnz +1 print "fail"
		       print "ok"),

	"t/tmp/stdlib/file already exists, as a regular file, and will not be automatically removed\n",
	"res.dir.absent skips files");
	ok -f "t/tmp/stdlib/file", "'file' still exists";

	stdlib_ok(qq(
	fn main
		set %a "t/tmp/stdlib/symlink"
		call res.dir.absent
		fs.stat %a
		jnz +1 print "fail"
		       print "ok"),

	"ok",
	"res.dir.absent can handle symlinks");
	ok !-l "t/tmp/stdlib/symlink", "'symlink' was removed";
};

subtest "res.dir.present" => sub {
	ok "tdb";
};

subtest "res.file.present" => sub {
	ok "tdb";
};

subtest "res.file.chown" => sub {
	ok "tdb";
};

subtest "res.file.chgrp" => sub {
	ok "tdb";
};

subtest "res.file.chmod" => sub {
	ok "tdb";
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
