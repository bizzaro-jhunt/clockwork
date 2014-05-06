#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::pendulum::common;

my $TESTS = "t/data/pn/fs";

# NB: If this changes, all the tests will need to change as well...
my $TEMP  = "t/tmp/pn/fs";
qx(rm -rf $TEMP; mkdir -p $TEMP);

qx(
	touch $TEMP/test.file
	mkdir $TEMP/test.dir
	ln -s $TEMP/test.file $TEMP/test.symlink
	mkfifo $TEMP/test.fifo
);

pendulum_ok "$TESTS/exists.pn", <<EOF, "exists.pn";
$TEMP/test.file exists
$TEMP/test.dir exists
$TEMP/test.symlink exists
$TEMP/test.fifo exists
/dev/null exists
/dev/ram0 exists
fin
EOF

pendulum_ok "$TESTS/stat.pn", <<EOF, "stat.pn";
$TEMP/test.file is a regular file
$TEMP/test.dir is a directory
$TEMP/test.symlink is a symlink
$TEMP/test.fifo is a fifo
/dev/null is a chardev
/dev/ram0 is a blockdev
fin
EOF

pendulum_ok "$TESTS/dirs.pn", <<EOF, "dirs.pn";
$TEMP/new.dir created
$TEMP/new.dir removed
fin
EOF

unlink "$TEMP/putfile";
pendulum_ok "$TESTS/put.pn", <<EOF, "put.pn";
OK
EOF

file_is "$TEMP/putfile", "0\n", "FS.PUT wrote the file";

done_testing;
