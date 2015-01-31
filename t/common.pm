package t::common;

use Test::More;
use Test::Builder;
use Text::Diff;
use File::Temp qw/tempfile/;
use base 'Exporter';
our @EXPORT_OK = qw/
	run_ok
	background_ok

	cw_shell_ok
	gencode_ok
	acls_ok
	resources_ok
	dependencies_ok

	pendulum_ok
	stdlib_ok
	disassemble_ok
	bdfa_ok
	command_ok

	set_facts
	put_file

	string_is
	file_is
	bdfa_file_is

	clean_tmp
/;
our @EXPORT = @EXPORT_OK;

my $T = Test::Builder->new;

my $PN = "./pn";
my $CWSH = "./cw-shell";
my $BDFA = "./bdfa";
my $COMMAND = "./TEST_cmd";

sub run_ok
{
	my ($cmd, $expect, $out, $err, $message) = @_;

	my $stdout = tempfile;
	my $stderr = tempfile;

	my $pid = fork;
	$T->BAIL_OUT("Failed to fork: $!") if $pid < 0;
	if ($pid == 0) {
		open STDOUT, ">&", $stdout
			or die "Failed to reopen stdout: $!\n";
		open STDERR, ">&", $stderr
			or die "Failed to reopen stderr: $!\n";
		exec $cmd
			or die "Failed to exec $cmd: $!\n";
	}

	local $SIG{ALRM} = sub { die "timed out\n" };
	alarm(10);
	eval {
		waitpid $pid, 0;
		alarm 0;
	};
	alarm 0;
	if ($@) {
		if ($@ =~ m/^timed out/) {
			kill 9, $pid;
			$T->ok(0, "$message: timed out $cmd");
			return 0;
		}
		$T->ok(0, "$message: exception raised - $@");
		return 0;
	}

	my $rc = $? >> 8;

	seek $stdout, 0, 0;
	$$out = do { local $/; <$stdout> };

	seek $stderr, 0, 0;
	$$err = do { local $/; <$stderr> };

	if ($rc != $expect) {
		$T->ok(0, "$message: $cmd returned exit code $rc (not $expect)");
		return 0;
	}

	$T->ok(1, $message);
	return 1;
}

sub background_ok
{
	my ($cmd, $message) = @_;

	my $pid = fork;
	$T->BAIL_OUT("Failed to fork for background run: $!") if $pid < 0;
	if ($pid == 0) {
		exec $cmd or die "Failed to exec $cmd: $!\n";
	}
	$T->ok(1, "$message (pid $pid)");
	return $pid;
}

sub _cw_shell_ok
{
	local $Test::Builder::Level = $Test::Builder::Level + 1;

	my ($expect, $message, %opts) = @_;
	$message ||= "cw shell run ok";
	$opts{timeout}  ||= 5;
	$opts{manifest} ||= "t/tmp/manifest.pol";
	$opts{facts}    ||= "t/tmp/facts.lst";
	$opts{command}  ||= "use host example";

	my $stdout = tempfile;
	my $stderr = tempfile;

	my $pid = fork;
	$T->BAIL_OUT("Failed to fork for cw shell run: $!") if $pid < 0;
	if ($pid == 0) {
		open STDOUT, ">&", $stdout
			or die "Failed to reopen stdout: $!\n";
		open STDERR, ">&", $stderr
			or die "Failed to reopen stderr: $!\n";
		exec $CWSH, '-qvv', '-f', $opts{facts}, $opts{manifest}, '-e', $opts{command}
			or die "Failed to exec $CWSH: $!\n";
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
			$T->ok(0, "$message: timed out running $CWSH");
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
	$T->diag("standard error output was:\n$errors") if $errors;

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

	$T->ok(1, $message);
	return 1;
}

sub cw_shell_ok     { _cw_shell_ok(@_); }
sub gencode_ok      { _cw_shell_ok(@_, command => "use host example; gencode"); }
sub acls_ok         { _cw_shell_ok(@_, command => "use host example; show acls"); }
sub resources_ok    { _cw_shell_ok(@_, command => "use host example; show resources"); }
sub dependencies_ok { _cw_shell_ok(@_, command => "use host example; show order"); }

sub pendulum_compile_ok
{
	my ($asm, $target, $message, %opts) = @_;
	$message ||= "compile";
	$opts{timeout} ||= 5;

	my $stdout = tempfile;
	my $stderr = tempfile;
	my $stdin  = tempfile;
	print $stdin $asm;
	seek $stdin, 0, 0;

	my $pid = fork;
	$T->BAIL_OUT("Failed to fork for pendulum compiler: $!") if $pid < 0;
	if ($pid == 0) {
		open STDOUT, ">&", $stdout
			or die "Failed to reopen stdout: $!\n";
		open STDERR, ">&", $stderr
			or die "Failed to reopen stderr: $!\n";
		open STDIN,  "<&", $stdin
			or die "Failed to reopen stdin: $!\n";
		exec $PN, @{$opts{args} || []}, '-Sg', '-' or die "Failed to exec $PN -Sg: $!\n";
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
			$T->ok(0, "$message: timed out running pendulum compiler");
			return 0;
		}
		$T->ok(0, "$message: exception raised - $@");
		return 0;
	}

	my $rc = $?;

	seek $stderr, 0, 0;
	my $errors = do { local $/; <$stderr> };
	if ($rc != 0 and defined $errors) {
		$T->diag("Standard error:");
		$T->diag($errors);
	}

	open my $fh, ">", $target
		or $T->BAIL_OUT("Failed to open target '$target' for writing: $!");

	seek $stdout, 0, 0;
	print $fh do { local $/; <$stdout> };
	close $fh;

	$T->ok($rc == 0, $message);
	return $rc == 0;
}

sub pendulum_ok
{
	my ($asm, $expect, $message, %opts) = @_;
	$message ||= "run ok";
	$opts{timeout} ||= 5;

	my (undef, $target) = tempfile;
	pendulum_compile_ok($asm, $target, "compile asm", %opts)
		or return 0;

	my $stdout = tempfile;
	my $stderr = tempfile;

	my $pid = fork;
	$T->BAIL_OUT("Failed to fork for pendulum interpreter run: $!") if $pid < 0;
	if ($pid == 0) {
		open STDOUT, ">&", $stdout
			or die "Failed to reopen stdout: $!\n";
		open STDERR, ">&", $opts{stderr} eq 'stdout' ? $stdout : $stderr
			or die "Failed to reopen stderr: $!\n";
		exec $PN, @{$opts{args} || []}, $target or die "Failed to exec $PN: $!\n";
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
	$T->diag("standard error output was:\n$errors") if $errors;

	$actual = $opts{postprocess}->($actual)
		if ($opts{postprocess});

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

	$T->ok(1, $message);
	return 1;
}

sub disassemble_ok { local $Test::Builder::Level = $Test::Builder::Level + 1;
                     pendulum_ok @_, args => ['-d']; }
sub stdlib_ok      { local $Test::Builder::Level = $Test::Builder::Level + 1;
                     pendulum_ok @_, args => [qw[--stdlib stdlib.pn --cover]],
                                     stderr => 'stdout'; }

sub command_ok
{
	my ($command, $expect, $message, %opts) = @_;
	$message ||= $command;
	$opts{timeout} ||= 5;

	my $stdout = tempfile;
	my $stderr = tempfile;

	my $pid = fork;
	$T->BAIL_OUT("Failed to fork for command run: $!") if $pid < 0;
	if ($pid == 0) {
		open STDOUT, ">&", $stdout
			or die "Failed to reopen stdout: $!\n";
		open STDERR, ">&", $stderr
			or die "Failed to reopen stderr: $!\n";
		exec $COMMAND, $command or die "Failed to exec $COMMAND: $!\n";
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
			$T->ok(0, "$message: timed out running command script");
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
		$T->ok(0, "$message: TEST_cmd returned non-zero exit code $rc");
		$T->diag("standard error output was:\n$errors") if $errors;
		return 0;
	}
	$T->diag("standard error output was:\n$errors") if $errors;

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
		$T->ok(0, "$message: TEST_cmd printed to standard error");
		$T->diag($errors);
		return 0;
	}

	$T->ok(1, $message);
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
	$T->diag("standard error output was:\n$errors") if $errors;

	$T->ok(1, $message);
	$T->diag("standard error output was:\n$errors") if $errors;
	return 1;
}

sub put_file
{
	my ($file, $contents) = @_;
	open my $fh, ">", $file
		or $T->BAIL_OUT("failed to create $file: $!");

	print $fh $contents;
	close $fh;
	return 1;
}

sub set_facts
{
	my ($file, %facts) = @_;
	put_file $file, join('', map { "$_=$facts{$_}\n" } keys %facts);
}

sub string_is
{
	my ($actual, $expect, $message) = @_;

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

	$T->ok(1, $message);
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

	string_is $actual, $expect, $message;
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

	$actual = join('', map { "$_\n" } sort split('\n', $actual));
	$expect = join('', map { "$_\n" } sort split('\n', $expect));

	string_is $actual, $expect, $message;
}

sub clean_tmp
{
	qx(rm -rf t/tmp; mkdir t/tmp);
}

1;
