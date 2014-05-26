#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use File::Find;
use File::Slurp qw/read_file/;
use t::common;

my ($UID, $GID) = (2345, 5432);

my $ROOT = "t/tmp/data/bdfa";
my $TEMP = "t/tmp/bdfa";
qx(rm -rf $TEMP; mkdir -p $TEMP);
qx(find $ROOT -name '*.keep' | xargs rm -f);
qx(rm -f $ROOT/src/special/fifo; mkfifo $ROOT/src/special/fifo);
qx(cd $ROOT/src/special && ln -fs file symlink);
qx(cd $ROOT/src/special && ln -f  file hardlink);

if (!$>) {
	qx(rm -f $ROOT/src/special/*dev);
	qx(mknod $ROOT/src/special/blockdev b 7 99); # /dev/loop99
	qx(mknod $ROOT/src/special/chardev  c 1  3); # /dev/null

	# set up owner/group overrides for testing
	qx(chown -R $UID:$GID $ROOT/src);
} else {
	$UID = $>;
	$GID = $(; $GID =~ s/ .*//;
}
my $UIDhx = sprintf "%08x", $UID;
my $GIDhx = sprintf "%08x", $GID;

sub bdfa
{
	my ($s) = @_;
	chomp($s);
	$s =~ s/\n-{50,}\n//sg;
	$s;
}

my %MTIMES = ();
find(sub {
	# Aug 29 1997 2:14am
	my $t = $MTIMES{$File::Find::name} || 872835240;
	utime $t, $t, $_;
}, "$ROOT/src");

my @create = ('-cf', "$TEMP/archive.bdf");
my @extract = ('-xf', "$TEMP/archive.bdf", '-C', "$TEMP/dest");
my @stat;

###############################################################
###############################################################
###############################################################

bdfa_ok [@create, "$ROOT/src/empty"], "empty archive";
bdfa_file_is "$TEMP/archive.bdf", bdfa(<<EOF), "empty archive contents";
BDFA0001000000000000000000000000000000000000000000000000
EOF



bdfa_ok [@create, "$ROOT/src/1dir"], "1dir archive";
bdfa_file_is "$TEMP/archive.bdf", bdfa(<<EOF), "1dir archive contents";
BDFA0000000041fd${UIDhx}${GIDhx}340668a80000000000000008test\0\0\0\0
--------------------------------------------------------
BDFA0001000000000000000000000000000000000000000000000000
EOF

qx(rm -rf $TEMP/dest; mkdir -p $TEMP/dest);
bdfa_ok [@extract], "1dir extraction";
ok -d "$TEMP/dest/test", "1dir - created 'test/' directory"
	or diag qx(ls -lR $TEMP/dest);
@stat = stat "$TEMP/dest/test";
is $stat[2], 040775, "test/ directory mode is correct";
is $stat[4], $UID, "test/ UID is correct";
is $stat[5], $GID, "test/ GID is correct";
if (!$>) {
	is $stat[9], 872835240, "test/ mtime is correct";
}



bdfa_ok [@create, "$ROOT/src/1file"], "1file archive";
bdfa_file_is "$TEMP/archive.bdf", bdfa(<<EOF), "1file archive contents";
BDFA0000000081b4${UIDhx}${GIDhx}340668a80000000000000008test\0\0\0\0
--------------------------------------------------------
BDFA0001000000000000000000000000000000000000000000000000
EOF

qx(rm -rf $TEMP/dest; mkdir -p $TEMP/dest);
bdfa_ok [@extract], "1file extraction";
ok -f "$TEMP/dest/test", "1file - created 'test' file"
	or diag qx(ls -lR $TEMP/dest);
@stat = stat "$TEMP/dest/test";
is $stat[2], 0100664, "test file mode is correct";
is $stat[4], $UID, "test file UID is correct";
is $stat[5], $GID, "test file GID is correct";
is $stat[7], 0, "test file is blank";
is read_file("$TEMP/dest/test"), "", "test file is really blank";



bdfa_ok [@create, "$ROOT/src/special"], "special archive";
# fifo should be ignored as !file && !dir
bdfa_file_is "$TEMP/archive.bdf", bdfa(<<EOF), "special archive contents";
BDFA0000000041fd${UIDhx}${GIDhx}340668a80000000000000008dir\0\0\0\0\0
--------------------------------------------------------
BDFA0000000081b4${UIDhx}${GIDhx}340668a80000000800000008file\0\0\0\0MY FILE
--------------------------------------------------------
BDFA0000000081b4${UIDhx}${GIDhx}340668a8000000080000000csymlink\0\0\0\0\0MY FILE
--------------------------------------------------------
BDFA0000000081b4${UIDhx}${GIDhx}340668a8000000080000000chardlink\0\0\0\0MY FILE
--------------------------------------------------------
BDFA0001000000000000000000000000000000000000000000000000
EOF

qx(rm -rf $TEMP/dest; mkdir -p $TEMP/dest);
bdfa_ok [@extract], "special extraction";
ok -d "$TEMP/dest/dir", "special - created 'dir' directory"
	or diag qx(ls -lR $TEMP/dest);
ok -f "$TEMP/dest/file", "special - created 'file' file"
	or diag qx(ls -lR $TEMP/dest);
is read_file("$TEMP/dest/file"), "MY FILE\n", "contents of 'file' file";
ok -f "$TEMP/dest/symlink", "special - created 'symlink' file"
	or diag qx(ls -lR $TEMP/dest);
is read_file("$TEMP/dest/symlink"), "MY FILE\n", "contents of 'symlink' file";
ok -f "$TEMP/dest/hardlink", "special - created 'hardlink' file"
	or diag qx(ls -lR $TEMP/dest);
is read_file("$TEMP/dest/hardlink"), "MY FILE\n", "contents of 'hardlink' file";



bdfa_ok [@create, "$ROOT/src/full"], "full archive";
bdfa_file_is "$TEMP/archive.bdf", bdfa(<<EOF), "full archive contents";
BDFA0000000041fd${UIDhx}${GIDhx}340668a80000000000000008dat\0\0\0\0\0
--------------------------------------------------------
BDFA0000000081b4${UIDhx}${GIDhx}340668a80000002a00000008file\0\0\0\0this file is exactly 42 (0x2a) bytes long
--------------------------------------------------------
BDFA0000000041fd${UIDhx}${GIDhx}340668a80000000000000004a\0\0\0
--------------------------------------------------------
BDFA0000000041fd${UIDhx}${GIDhx}340668a80000000000000008a/b\0\0\0\0\0
--------------------------------------------------------
BDFA0000000041fd${UIDhx}${GIDhx}340668a80000000000000008a/b/c\0\0\0
--------------------------------------------------------
BDFA0000000041fd${UIDhx}${GIDhx}340668a8000000000000000ca/b/c/d\0\0\0\0\0
--------------------------------------------------------
BDFA0000000081b4${UIDhx}${GIDhx}340668a80000002700000018a/b/c/d/deepfile.txt\0\0\0\0this file is several directories deep!
--------------------------------------------------------
BDFA0000000041fd${UIDhx}${GIDhx}340668a8000000000000004cthis-is-a-really-long-directory-name-for-testing-limits-of-the-bdfa-format\0\0
--------------------------------------------------------
BDFA0000000081b4${UIDhx}${GIDhx}340668a80000003400000010multiline.txt\0\0\0this is a multiline file
it has newlines and stuff!
--------------------------------------------------------
BDFA0001000000000000000000000000000000000000000000000000
EOF

qx(rm -rf $TEMP);
done_testing;
