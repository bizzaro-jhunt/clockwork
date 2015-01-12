#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use File::Find;
use File::Slurp qw/read_file/;
use t::common;

my ($UID, $GID) = (2345, 5432);
if ($>) { $UID = $>; $GID = $(; $GID =~ s/ .*//; }
my $UIDhx = sprintf "%08x", $UID;
my $GIDhx = sprintf "%08x", $GID;

sub bdfa
{
	my ($s) = @_;
	chomp($s);
	$s =~ s/\n-{50,}\n//sg;
	$s;
}

sub prep
{
	find(sub {
		# Aug 29 1997 2:14am
		utime 872835240, 872835240, $_;
		chmod 0755, $_ if -d $_;
		chmod 0644, $_ if -f $_;
	}, "t/tmp/bdfa");
}

my @create = ('-cf', "t/tmp/archive.bdf");
my @extract = ('-xf', "t/tmp/archive.bdf", '-C', "t/tmp/dest");
my @stat;

###############################################################
###############################################################
###############################################################

subtest "empty archive" => sub {
	qx{ rm -rf t/tmp/bdfa; mkdir -p t/tmp/bdfa };
	bdfa_ok [@create, "t/tmp/bdfa"], "empty archive";
	bdfa_file_is "t/tmp/archive.bdf", bdfa(<<EOF), "empty archive contents";
BDFA0001000000000000000000000000000000000000000000000000
EOF
};

###############################################################

subtest "archive a single directory" => sub {
	qx{ rm -rf t/tmp/bdfa; mkdir -p t/tmp/bdfa/test };
	qx { chown -R $UID:$GID t/tmp/bdfa } if !$>;
	prep;

	bdfa_ok [@create, "t/tmp/bdfa"], "created archive";
	bdfa_file_is "t/tmp/archive.bdf", bdfa(<<EOF), "archive contents";
BDFA0000000041ed${UIDhx}${GIDhx}340668a80000000000000008test\0\0\0\0
--------------------------------------------------------
BDFA0001000000000000000000000000000000000000000000000000
EOF

	qx{ rm -rf t/tmp/dest; mkdir -p t/tmp/dest };
	bdfa_ok [@extract], "extraction";
	ok -d "t/tmp/dest/test", "extraction created 'test/' directory"
		or diag qx(ls -lR t/tmp/dest);
	@stat = stat "t/tmp/dest/test";
	is $stat[2], 040755, "test/ directory mode is correct";
	is $stat[4], $UID, "test/ UID is correct";
	is $stat[5], $GID, "test/ GID is correct";
	if (!$>) {
		is $stat[9], 872835240, "test/ mtime is correct";
	}
};

###############################################################

subtest "archive single file" => sub {
	qx{ rm -rf t/tmp/bdfa; mkdir -p t/tmp/bdfa; touch t/tmp/bdfa/test };
	qx { chown -R $UID:$GID t/tmp/bdfa } if !$>;
	prep;

	bdfa_ok [@create, "t/tmp/bdfa"], "created archive";
	bdfa_file_is "t/tmp/archive.bdf", bdfa(<<EOF), "archive contents";
BDFA0000000081a4${UIDhx}${GIDhx}340668a80000000000000008test\0\0\0\0
--------------------------------------------------------
BDFA0001000000000000000000000000000000000000000000000000
EOF

	qx(rm -rf t/tmp/dest; mkdir -p t/tmp/dest);
	bdfa_ok [@extract], "extraction";
	ok -f "t/tmp/dest/test", "created 'test' file"
		or diag qx(ls -lR t/tmp/dest);
	@stat = stat "t/tmp/dest/test";
	is $stat[2], 0100644, "test file mode is correct";
	is $stat[4], $UID, "test file UID is correct";
	is $stat[5], $GID, "test file GID is correct";
	is $stat[7], 0, "test file is blank";
	is read_file("t/tmp/dest/test"), "", "test file is really blank";
};

###############################################################

subtest "archive with special files" => sub {
	qx{ rm -rf t/tmp/bdfa; mkdir -p t/tmp/bdfa;
	    mkdir  t/tmp/bdfa/dir;
	    mkfifo t/tmp/bdfa/fifo;
	    echo "MY FILE" >t/tmp/bdfa/file;
	   (cd t/tmp/bdfa; ln -sf file symlink; ln file hardlink); };
	qx { chown -R $UID:$GID t/tmp/bdfa } if !$>;
	prep;

	bdfa_ok [@create, "t/tmp/bdfa"], "created archive";
	# fifo should be ignored as !file && !dir
	bdfa_file_is "t/tmp/archive.bdf", bdfa(<<EOF), "archive contents";
BDFA0000000041ed${UIDhx}${GIDhx}340668a80000000000000008dir\0\0\0\0\0
--------------------------------------------------------
BDFA0000000081a4${UIDhx}${GIDhx}340668a80000000800000008file\0\0\0\0MY FILE
--------------------------------------------------------
BDFA0000000081a4${UIDhx}${GIDhx}340668a8000000080000000csymlink\0\0\0\0\0MY FILE
--------------------------------------------------------
BDFA0000000081a4${UIDhx}${GIDhx}340668a8000000080000000chardlink\0\0\0\0MY FILE
--------------------------------------------------------
BDFA0001000000000000000000000000000000000000000000000000
EOF

	qx(rm -rf t/tmp/dest; mkdir -p t/tmp/dest);
	bdfa_ok [@extract], "extraction";
	ok -d "t/tmp/dest/dir", "created 'dir' directory"
		or diag qx(ls -lR t/tmp/dest);
	ok -f "t/tmp/dest/file", "created 'file' file"
		or diag qx(ls -lR t/tmp/dest);
	is read_file("t/tmp/dest/file"), "MY FILE\n", "contents of 'file' file";
	ok -f "t/tmp/dest/symlink", "created 'symlink' file"
		or diag qx(ls -lR t/tmp/dest);
	is read_file("t/tmp/dest/symlink"), "MY FILE\n", "contents of 'symlink' file";
	ok -f "t/tmp/dest/hardlink", "created 'hardlink' file"
		or diag qx(ls -lR t/tmp/dest);
	is read_file("t/tmp/dest/hardlink"), "MY FILE\n", "contents of 'hardlink' file";
};

subtest "full archive" => sub {
	qx{ rm -rf t/tmp/bdfa; mkdir -p t/tmp/bdfa;
	    mkdir -p t/tmp/bdfa/a/b/c/d
	    mkdir -p t/tmp/bdfa/dat
	    mkdir -p t/tmp/bdfa/this-is-a-really-long-directory-name-for-testing-limits-of-the-bdfa-format };
	qx { chown -R $UID:$GID t/tmp/bdfa } if !$>;
	qx { mknod t/tmp/bdfa/blockdev b 7 99;
	     mknod t/tmp/bdfa/chardev  c 1  3; } if !$>;
	put_file "t/tmp/bdfa/a/b/c/d/deepfile.txt", <<EOF;
this file is several directories deep!
EOF
	put_file "t/tmp/bdfa/file", <<EOF;
this file is exactly 42 (0x2a) bytes long
EOF
	put_file "t/tmp/bdfa/multiline.txt", <<EOF;
this is a multiline file
it has newlines and stuff!
EOF
	prep;

	bdfa_ok [@create, "t/tmp/bdfa"], "create archive";
	bdfa_file_is "t/tmp/archive.bdf", bdfa(<<EOF), "archive contents";
BDFA0000000041ed${UIDhx}${GIDhx}340668a80000000000000008dat\0\0\0\0\0
--------------------------------------------------------
BDFA0000000081a4${UIDhx}${GIDhx}340668a80000002a00000008file\0\0\0\0this file is exactly 42 (0x2a) bytes long
--------------------------------------------------------
BDFA0000000041ed${UIDhx}${GIDhx}340668a80000000000000004a\0\0\0
--------------------------------------------------------
BDFA0000000041ed${UIDhx}${GIDhx}340668a80000000000000008a/b\0\0\0\0\0
--------------------------------------------------------
BDFA0000000041ed${UIDhx}${GIDhx}340668a80000000000000008a/b/c\0\0\0
--------------------------------------------------------
BDFA0000000041ed${UIDhx}${GIDhx}340668a8000000000000000ca/b/c/d\0\0\0\0\0
--------------------------------------------------------
BDFA0000000081a4${UIDhx}${GIDhx}340668a80000002700000018a/b/c/d/deepfile.txt\0\0\0\0this file is several directories deep!
--------------------------------------------------------
BDFA0000000041ed${UIDhx}${GIDhx}340668a8000000000000004cthis-is-a-really-long-directory-name-for-testing-limits-of-the-bdfa-format\0\0
--------------------------------------------------------
BDFA0000000081a4${UIDhx}${GIDhx}340668a80000003400000010multiline.txt\0\0\0this is a multiline file
it has newlines and stuff!
--------------------------------------------------------
BDFA0001000000000000000000000000000000000000000000000000
EOF
};

done_testing;
