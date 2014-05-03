#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use Test::Deep;
use Text::Diff;
use File::Temp qw/tempfile/;

my $PN = "./pn";

sub pendulum_ok
{
	local $Test::Builder::Level = $Test::Builder::Level + 1;

	my ($script, $output, $message, %opts) = @_;
	$message ||= "$script run ok";
	$opts{timeout} ||= 5;

	my $stdout = tempfile;
	my $stderr = tempfile;

	my $pid = fork;
	BAIL_OUT "Failed to fork for pendulum run: $!" if $pid < 0;
	if (!$pid) {
		open STDOUT, ">&", $stdout
			or die "Failed to reopen stdout: $!\n";
		open STDERR, ">&", $stderr
			or die "Failed to reopen stderr: $!\n";
		exec $PN, $script or die "Failed to exec $PN: $!\n";
	}

	local $SIG{ALRM} = sub { die "timed out\n" };
	alarm($opts{timeout});
	eval {
		waitpid $pid, 0;
		alarm 0;
	};
	alarm 0;
	if ($@) {
		if ($@ =~ m/^timed out/) {
			kill 9, $pid;
			fail "$message: timed out running pendulum script";
			return 0;
		}
		fail "$message: exception raised - $@";
		return 0;
	}

	my $rc = $? >> 8;

	seek $stdout, 0, 0;
	my $actual = do { local $/; <$stdout> };

	seek $stderr, 0, 0;
	my $errors = do { local $/; <$stderr> };

	if ($rc != 0) {
		fail "$message: $script returned non-zero exit code $rc";
		diag "standard error output was:\n$errors" if $errors;
		return 0;
	}

	my $diff = diff \$actual, \$output, {
		FILENAME_A => 'actual-output',    MTIME_A => time,
		FILENAME_B => 'expected-output',  MTIME_B => time,
		STYLE      => 'Unified',
		CONTEXT    => 8
	};

	if ($diff) {
		fail $message;
		diag "differences follow:\n$diff";
		diag "standard error output was:\n$errors" if $errors;
		return 0;
	}

	if ($errors) {
		fail "$message: $script printed to standard error";
		diag $errors;
		return 0;
	}

	pass $message;
	return 1;
}

###########################################

my $TESTS = "t/data/pn";

pendulum_ok "$TESTS/simple.pn", <<EOF, "simple.pn";
Hello, World!
EOF

pendulum_ok "$TESTS/halt.pn", <<EOF, "halt.pn";
Calling Halt...
EOF

pendulum_ok "$TESTS/implicit-halt.pn", <<EOF, "implicit-halt.pn";
Falling off the end of the program...
EOF

pendulum_ok "$TESTS/noop.pn", <<EOF, "noop.pn";
NOOPs are okay by me
EOF

pendulum_ok "$TESTS/jump.pn", <<EOF, "jump.pn";
\@start
\@previous
\@next
fin
EOF

pendulum_ok "$TESTS/ok.pn", <<EOF, "ok.pn";
OK? jumps if R register is non-zero
OK? continues if R register is zero
EOF

pendulum_ok "$TESTS/equality.pn", <<EOF, "equality.pn";
EQ? 42 54 was false
NE? 42 54 was true
GT? 42 54 was false
GTE? 42 54 was false
LT? 42 54 was true
LTE? 42 54 was true

EQ? 29 29 was true
NE? 29 29 was false
GT? 29 29 was false
GTE? 29 29 was true
LT? 29 29 was false
LTE? 29 29 was true

EQ? 1860 1776 was false
NE? 1860 1776 was true
GT? 1860 1776 was true
GTE? 1860 1776 was true
LT? 1860 1776 was false
LTE? 1860 1776 was false
fin
EOF

pendulum_ok "$TESTS/cmp.pn", <<EOF, "cmp.pn";
CMP? works
CMP? is case-sensitive
CMP? treats same strings as equal
fin
EOF

pendulum_ok "$TESTS/vcheck.pn", <<EOF, "vcheck.pn";
pendulum is at least v0
pendulum is not yet v9999
EOF

done_testing;
