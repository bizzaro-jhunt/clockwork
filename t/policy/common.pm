package t::policy::common;

use Test::More;
use Test::Builder;
use Text::Diff;
use File::Temp qw/tempfile/;
use base 'Exporter';
our @EXPORT_OK = qw/
	gencode_ok
/;
our @EXPORT = @EXPORT_OK;

my $T = Test::Builder->new;

my $CWPOL = "./cwpol";
my $MANIFEST = "t/data/policy/manifest.pol";

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
		exec $CWPOL, $MANIFEST, '-e', "$commands; gencode"
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

1;
