#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use File::Slurp qw/write_file/;
use t::pendulum::common;

# NB: If these change, all the tests will need to change as well...
my $TESTS = "t/data/pn/augeas";
my $TEMP  = "t/tmp/pn/augeas";

my $HOSTS_FILE = <<EOF;
127.0.0.1 localhost localhost.localdomain

# The following lines are desirable for IPv6 capable hosts
::1     ip6-localhost ip6-loopback
fe00::0 ip6-localnet
ff00::0 ip6-mcastprefix
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters

10.10.0.1 host.remove-me
EOF

sub revert
{
	qx(rm -rf $TEMP; mkdir -p $TEMP/etc);
	write_file "$TEMP/etc/hosts", $HOSTS_FILE;
}

sub no_change
{
	my ($test) = @_;
	local $Test::Builder::Level = $Test::Builder::Level + 1;
	file_is "$TEMP/etc/hosts", $HOSTS_FILE, "no changes to etc/hosts, after $test";
}

revert;
pendulum_ok "$TESTS/noop.pn", <<'EOF', "noop.pn";
initialized
torn down
EOF
no_change "noop.pn";

revert;
pendulum_ok "$TESTS/update.pn", <<'EOF', "update.pn";
initialized
torn down
EOF
file_is "$TEMP/etc/hosts", <<'EOF', "etc/hosts changed";
127.0.0.1 localhost localhost.localdomain

# The following lines are desirable for IPv6 capable hosts
::1     ip6-localhost ip6-loopback
fe00::0 ip6-localnet
ff00::0 ip6-mcastprefix
ff02::1 ip6-allnodes
ff02::2 ip6-allrouters

10.8.7.9	new.host.example
EOF

revert;
pendulum_ok "$TESTS/get.pn", <<'EOF', "get.pn";
hosts/3/canonical is ip6-localnet
hosts/3/ipaddr is fe00::0
found host at /files/etc/hosts/1
EOF

done_testing;
