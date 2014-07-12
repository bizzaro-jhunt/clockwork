#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use POSIX;

my $output;
my $ppid = getppid;
my  $gid = $ENV{SCHROOT_GID} ||  getgid;
my $egid = $ENV{SCHROOT_GID} || getegid;
my  $uid = $ENV{SCHROOT_UID} ||  getuid;
my $euid = $ENV{SCHROOT_UID} || geteuid;

diag explain \%ENV;

$output = qx(./TEST_ps $$);
is $?, 0, "`./TEST_ps $$` ran ok";
like $output, qr{^pid $$; ppid $ppid; state [RS]; uid $uid/$euid; gid $gid/$egid$},
	"cw_proc_stat is gathering data properly";

my $pid = 2;
$pid++ while -e "/proc/$pid/status";
note "Testing with PID $pid";
$output = qx(./TEST_ps $pid);
isnt $?, 0, "cw_proc_stat treats unknown PID as a failure";

done_testing;
