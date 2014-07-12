#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use POSIX;

my $output;
my $ppid = getppid;
my  $gid = getgid;
my $egid = getegid;

$output = qx(./TEST_ps $$);
is $?, 0, "`./TEST_ps $$` ran ok";
like $output, qr{^pid $$; ppid $ppid; state [RS]; uid $>/$<; gid $gid/$egid$},
	"cw_proc_stat is gathering data properly";

my $pid = 2;
$pid++ while -e "/proc/$pid/status";
note "Testing with PID $pid";
$output = qx(./TEST_ps $pid);
isnt $?, 0, "cw_proc_stat treats unknown PID as a failure";

done_testing;
