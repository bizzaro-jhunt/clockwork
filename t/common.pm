package t::common;

use Test::More;
use Test::Builder;
use Text::Diff;
use File::Temp qw/tempfile/;
use base 'Exporter';
our @EXPORT_OK = qw/
	gencode_ok
	pendulum_ok
	bdfa_ok

	file_is
	bdfa_file_is
/;
our @EXPORT = @EXPORT_OK;

my $T = Test::Builder->new;

my $PN = "./pn";
my $CWPOL = "./cwpol";
my $BDFA = "./bdfa";
my $MANIFEST = "t/tmp/data/policy/manifest.pol";

sub gencode_ok
{
	my ($commands, $expect, $message, %opts) = @_;
	$message ||= "cwpol run ok";
	$opts{timeout} ||= 5;

	my $stdout = tempfile;
	my $stderr = tempfile;

	my $pid = fork;
	$T->BAIL_OUT("Failed to fork for cwpol run: $!") if $pid < 0;
	if ($pid == 0) {
		open STDOUT, ">&", $stdout
			or die "Failed to reopen stdout: $!\n";
		open STDERR, ">&", $stderr
			or die "Failed to reopen stderr: $!\n";
		exec $CWPOL, '-q', $MANIFEST, '-e', "$commands; gencode"
			or die "Failed to exec $CWPOL: $!\n";
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
			$T->ok(0, "$message: timed out running $CWPOL");
			return 0;
		}
		$T->ok(0, "$message: exception raised - $@");
		return 0;
	}

	my $rc = $? >> 8;

	seek $stdout, 0, 0;
	my $actual = do { local $/; <$stdout> };

	seek $stderr, 0, 0;
	my $errors = do { local $/; <$stderr> };

	if ($rc != 0) {
		$T->ok(0, "$message: $commands returned non-zero exit code $rc");
		$T->diag("standard error output was:\n$errors") if $errors;
		return 0;
	}

	my $diff = diff \$actual, \$expect, {
		FILENAME_A => 'actual-output',    MTIME_A => time,
		FILENAME_B => 'expected-output',  MTIME_B => time,
		STYLE      => 'Unified',
		CONTEXT    => 8
	};

	if ($diff) {
		$T->ok(0, $message);
		$T->diag("differences follow:\n$diff");
		$T->diag("standard error output was:\n$errors") if $errors;
		return 0;
	}

	pass $message;
	return 1;
}

sub pendulum_ok
{
	my ($script, $expect, $message, %opts) = @_;
	$message ||= "$script run ok";
	$opts{timeout} ||= 5;

	my $stdout = tempfile;
	my $stderr = tempfile;

	my $pid = fork;
	$T->BAIL_OUT("Failed to fork for pendulum run: $!") if $pid < 0;
	if ($pid == 0) {
		open STDOUT, ">&", $stdout
			or die "Failed to reopen stdout: $!\n";
		open STDERR, ">&", $stderr
			or die "Failed to reopen stderr: $!\n";
		exec $PN, '-q', $script or die "Failed to exec $PN: $!\n";
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
			$T->ok(0, "$message: timed out running pendulum script");
			return 0;
		}
		$T->ok(0, "$message: exception raised - $@");
		return 0;
	}

	my $rc = $? >> 8;

	seek $stdout, 0, 0;
	my $actual = do { local $/; <$stdout> };

	seek $stderr, 0, 0;
	my $errors = do { local $/; <$stderr> };

	if ($rc != 0) {
		$T->ok(0, "$message: $script returned non-zero exit code $rc");
		$T->diag("standard error output was:\n$errors") if $errors;
		return 0;
	}

	my $diff = diff \$actual, \$expect, {
		FILENAME_A => 'actual-output',    MTIME_A => time,
		FILENAME_B => 'expected-output',  MTIME_B => time,
		STYLE      => 'Unified',
		CONTEXT    => 8
	};

	if ($diff) {
		$T->ok(0, $message);
		$T->diag("differences follow:\n$diff");
		$T->diag("standard error output was:\n$errors") if $errors;
		return 0;
	}

	if ($errors) {
		$T->ok(0, "$message: $script printed to standard error");
		$T->diag($errors);
		return 0;
	}

	pass $message;
	return 1;
}

sub bdfa_ok
{
	my ($args, $message, %opts) = @_;
	$message ||= "bdfa run ok";
	$opts{timeout} ||= 5;

	my $stdout = tempfile;
	my $stderr = tempfile;

	my $pid = fork;
	$T->BAIL_OUT("Failed to fork for bdfa run: $!") if $pid < 0;
	if ($pid == 0) {
		open STDOUT, ">&", $stdout
			or die "Failed to reopen stdout: $!\n";
		open STDERR, ">&", $stderr
			or die "Failed to reopen stderr: $!\n";
		exec $BDFA, @$args
			or die "Failed to exec $BDFA: $!\n";
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
			$T->ok(0, "$message: timed out running $BDFA");
			return 0;
		}
		$T->ok(0, "$message: exception raised - $@");
		return 0;
	}

	my $rc = $? >> 8;

	seek $stdout, 0, 0;
	my $actual = do { local $/; <$stdout> };

	seek $stderr, 0, 0;
	my $errors = do { local $/; <$stderr> };

	if ($rc != 0) {
		$T->ok(0, "$message: bdfa returned non-zero exit code $rc");
		$T->diag("standard error output was:\n$errors") if $errors;
		return 0;
	}

	pass $message;
	$T->diag("standard error output was:\n$errors") if $errors;
	return 1;
}

sub file_is
{
	my ($path, $expect, $message) = @_;
	$message ||= "$path contents";

	open my $fh, "<", $path or do {
		$T->ok(0, "Failed to open $path for reading: $!");
		return 0;
	};
	my $actual = do { local $/; <$fh> };
	close $fh;

	$actual .= "\n" unless $actual =~ m/\n$/;
	$expect .= "\n" unless $expect =~ m/\n$/;

	$actual =~ s/\0/\\0/g;
	$expect =~ s/\0/\\0/g;

	my $diff = diff \$actual, \$expect, {
		FILENAME_A => 'actual-output',    MTIME_A => time,
		FILENAME_B => 'expected-output',  MTIME_B => time,
		STYLE      => 'Unified',
		CONTEXT    => 8
	};

	if ($diff) {
		$T->ok(0, $message);
		$T->diag("differences follow:\n$diff");
		return 0;
	}

	pass $message;
	return 1;
}

sub bdfa_file_is
{
	my ($path, $expect, $message) = @_;
	$message ||= "$path contents";

	open my $fh, "<", $path or do {
		$T->ok(0, "Failed to open $path for reading: $!");
		return 0;
	};
	my $actual = do { local $/; <$fh> };
	close $fh;

	$actual .= "\n" unless $actual =~ m/\n$/;
	$expect .= "\n" unless $expect =~ m/\n$/;

	$actual =~ s/\0/\\0/g;
	$expect =~ s/\0/\\0/g;

	$actual =~ s/(.)BDFA/$1\nBDFA/g;
	$expect =~ s/(.)BDFA/$1\nBDFA/g;

	my $diff = diff \$actual, \$expect, {
		FILENAME_A => 'actual-output',    MTIME_A => time,
		FILENAME_B => 'expected-output',  MTIME_B => time,
		STYLE      => 'Unified',
		CONTEXT    => 8
	};

	if ($diff) {
		$T->ok(0, $message);
		$T->diag("differences follow:\n$diff");
		return 0;
	}

	pass $message;
	return 1;
}

1;
