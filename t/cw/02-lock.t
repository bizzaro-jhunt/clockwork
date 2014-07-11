#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use POSIX;

my $LOCK = "/tmp/cw.lock.test.$$";
sub reset_lock
{
	delete $ENV{TEST_LOCK_FLAGS};
	chmod 0644, $LOCK; unlink $LOCK;
	return unless $_[0];

	open my $fh, ">", $LOCK
		or BAIL_OUT "Failed to open $LOCK for writing: $!";
	print $fh $_[0];
	close $fh;
}

reset_lock;
is qx(./TEST_lock $LOCK 2>&1), "acquired $LOCK\n",
	"acquired a lock when no one was looking";

reset_lock "garbage lock data\n";
is qx(./TEST_lock $LOCK 2>&1), "FAILED: $LOCK in use by <invalid lock file>\n",
	"cw_lock() can detect poorly formatted lock files (and preserve them!)";

my $time = strftime("%Y-%m-%d %H:%M:%S%z", localtime(1405048555));
reset_lock "LOCK p$$ u$> t1405048555";
is qx(./TEST_lock $LOCK 2>&1), "FAILED: $LOCK in use by PID $$, $ENV{USER}($>); locked $time\n",
	"cw_lock() honors locks held by living processes";

my $UID = 1000;
$UID++ while getpwuid($UID);
reset_lock "LOCK p$$ u$UID t1405048555";
$ENV{TEST_LOCK_FLAGS} = "1"; # skip EUID
is qx(./TEST_lock $LOCK 2>&1), "FAILED: $LOCK in use by PID $$, <unknown>($UID); locked $time\n",
	"cw_lock_info() handles unknown EUIDs for name lookup";

done_testing;
