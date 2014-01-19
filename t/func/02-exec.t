#!perl
use strict;
use warnings;

use Test::More;
use Test::Deep;
use Test::Builder;

sub exec_ok
{
	my ($flags, $command, $expect, $msg) = @_;
	local $Test::Builder::Level = $Test::Builder::Level + 1;

	my $cmd = "./t/helpers/executor $flags '$command'";
	open my $fh, "-|", $cmd
		or BAIL_OUT "Failed to exec $cmd: $!";
	my $out = do { local $/; <$fh> };
	close $fh;

	is $out, $expect, $msg;
}


my $out;
$out = <<EOF;
rc = 0
STDOUT
------
to stdout
STDERR
------
------
EOF

exec_ok "both", qq(echo "to stdout"), $out,
        "Catch stdout stream via 'both'";

exec_ok "stdout", qq(echo "to stdout"), $out,
        "Catch stdout stream via 'stdout'";

$out = <<EOF;
rc = 0
STDOUT
------
STDERR
------
to stderr!
------
EOF

exec_ok "both", qq(echo >&2 "to stderr!"), $out,
        "Catch stderr stream via 'both'";

exec_ok "stderr", qq(echo >&2 "to stderr!"), $out,
        "Catch stderr stream via 'stderr'";

$out = <<EOF;
rc = 0
STDOUT
------
STDERR
------
------
EOF

exec_ok "none", qq(echo >&2 "this should not be captured"), $out,
        "Turn off all output capture (write to stderr)";

exec_ok "none", qq(echo "this should not be captured"), $out,
        "Turn off all output capture (write to stdout)";

$out = <<EOF;
rc = 0
STDOUT
------
failing
STDERR
------
FAILING
------
EOF

exec_ok "both", qq(/bin/sh -c "echo failing; echo >&2 FAILING"), $out,
        "Capture output simultaneously";

$out = <<EOF;
rc = 24
STDOUT
------
STDERR
------
------
EOF

exec_ok "both", qq(/bin/sh -c "exit 24"), $out,
        "non-zero exit code is handled";

$out = <<EOF;
rc = -1
STDOUT
------
STDERR
------
------
EOF

exec_ok "both", q(/bin/sh -c "kill -INT $$"), $out,
        "Terminate-via-signal is handled";

done_testing;
