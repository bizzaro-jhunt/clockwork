#!perl
use strict;
use warnings;

use Test::More;
use Test::Deep;
use File::Slurp qw/read_file/;
use Test::Builder;

sub prompt_ok
{
	my ($prompt, $value, $msg) = @_;
	local $Test::Builder::Level = $Test::Builder::Level + 1;

	mkdir "t/tmp";
	my $cmd = "./t/helpers/prompter".(defined($prompt) ? " '$prompt'":'');
	open my $fh, "|-", "$cmd 2>&1 > t/tmp/prompter"
		or BAIL_OUT "Failed to exec $cmd: $!";

	print $fh "$value\n";
	close $fh;
	$prompt ||= '';
	is read_file("t/tmp/prompter"), "$prompt'$value'\n", $msg;
	unlink "t/tmp/prompter";
}


prompt_ok 'prompt> ',
          'a value to enter',
          'simple prompt';

prompt_ok undef,
          'xyzzy',
          'no prompt';

prompt_ok 'BIG: ',
          ('X' x 8191).' - this is a reallllly long value!',
          'long prompt';

done_testing;
