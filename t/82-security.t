#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

##########################################################################

subtest "cogd: invalid configuration" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:3333
cert.1   t/tmp/master.1.pub

security.cert t/tmp/badcert
EOF
	put_file "t/tmp/master.1.pub", <<EOF;
id  master.host.1
pub 76f5d6f76aef22a22f15fd8d90c837756d1ea0a42907fb0c58a999d803954377
EOF
	put_file "t/tmp/badcert", <<EOF;
this is not a certificate file
it is instead a text file
have fun with that!
EOF
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Fvc t/tmp/cogd.conf", 1, \$stdout, \$stderr,
		"Run cogd with an invalid certificate";

	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	string_is $stderr, <<EOF, "invalid: Standard error output";
t/tmp/badcert: Invalid Clockwork certificate
EOF
	string_is $stdout, '', "invalid: No standard output";
};

##########################################################################

subtest "cogd: missing certificate" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:3333
cert.1   t/tmp/master.1.pub

security.cert /path/to/nowhere
EOF
	put_file "t/tmp/master.1.pub", <<EOF;
id  master.host.1
pub 76f5d6f76aef22a22f15fd8d90c837756d1ea0a42907fb0c58a999d803954377
EOF
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Fvc t/tmp/cogd.conf", 1, \$stdout, \$stderr,
		"Run cogd with no certificate";

	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	string_is $stderr, <<EOF, "nocert: Standard error output";
/path/to/nowhere: No such file or directory
EOF

	string_is $stdout, '', "nocert: No standard output";
};

##########################################################################

subtest "cogd: cert with no private certificate" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:3333
cert.1   t/tmp/master.1.pub

security.cert t/tmp/cert
EOF
	put_file "t/tmp/master.1.pub", <<EOF;
id  master.host.1
pub 76f5d6f76aef22a22f15fd8d90c837756d1ea0a42907fb0c58a999d803954377
EOF
	put_file "t/tmp/cert", <<EOF;
id  pubonly.test
pub aea625f90ac478b9bc9649f6ab74e5f095bab8522f107b1a6d701e83c41cfc4b
EOF
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Fvc t/tmp/cogd.conf", 1, \$stdout, \$stderr,
		"Run cogd with pub-only certificate";

	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	string_is $stderr, <<EOF, "pubonly: Standard error output";
t/tmp/cert: No secret key found in certificate
EOF

	string_is $stdout, '', "pubonly: No standard output";
};

##########################################################################

subtest "cogd: missing identity" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:3333
cert.1   t/tmp/master.1.pub

security.cert t/tmp/cert
EOF
	put_file "t/tmp/master.1.pub", <<EOF;
id  master.host.1
pub 76f5d6f76aef22a22f15fd8d90c837756d1ea0a42907fb0c58a999d803954377
EOF
	put_file "t/tmp/cert", <<EOF;
pub aea625f90ac478b9bc9649f6ab74e5f095bab8522f107b1a6d701e83c41cfc4b
sec aea625f90ac478b9bc9649f6ab74e5f095bab8522f107b1a6d701e83c41cfc4b
EOF
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Fvc t/tmp/cogd.conf", 1, \$stdout, \$stderr,
		"Run cogd with anonymous certificate";

	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	string_is $stderr, <<EOF, "noident: Standard error output";
t/tmp/cert: No identity found in certificate
EOF

	string_is $stdout, '', "noident: No standard output";
};

##########################################################################

subtest "cogd: no master certificate" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:3333
# no cert.1

security.cert t/tmp/cert
EOF
	put_file "t/tmp/cert", <<EOF;
pub aea625f90ac478b9bc9649f6ab74e5f095bab8522f107b1a6d701e83c41cfc4b
sec aea625f90ac478b9bc9649f6ab74e5f095bab8522f107b1a6d701e83c41cfc4b
EOF
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Fvc t/tmp/cogd.conf", 2, \$stdout, \$stderr,
		"Run cogd with no configured master certificates";

	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	string_is $stderr, <<EOF, "nomastercert: Standard error output";
master.1 (127.0.0.1:3333) has no matching certificate (cert.1)
EOF

	string_is $stdout, '', "nomastercert: No standard output";
};

##########################################################################

subtest "cogd: invalid master certificate" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:3333
cert.1 t/tmp/master.pub

security.cert t/tmp/master.1
EOF
	put_file "t/tmp/master.pub", <<EOF;
# no pubkey in here!
EOF
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Fvc t/tmp/cogd.conf", 2, \$stdout, \$stderr,
		"Run cogd with malformed master certificates";

	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	string_is $stderr, <<EOF, "badmastercert: Standard error output";
cert.1 t/tmp/master.pub: Invalid Clockwork certificate
EOF

	string_is $stdout, '', "badmastercert: No standard output";
};

##########################################################################

subtest "clockd: invalid certificate" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/clockd.conf", <<EOF;
security.cert t/tmp/cert
EOF
	put_file "t/tmp/cert", <<EOF;
this is not a certificate file
it is instead a text file
have fun with that!
EOF
	my ($stdout, $stderr);
	local $ENV{TEST_CLOCKD_BAIL_EARLY} = 1;
	run_ok "./TEST_clockd -Fvc t/tmp/clockd.conf", 1, \$stdout, \$stderr,
		"Run clockd with an invalid certificate";

	$stdout =~ s/^clockd\[\d+\]\s*//mg;
	string_is $stdout, <<EOF, "invalid: Standard output";
t/tmp/cert: Invalid Clockwork certificate
EOF

	string_is $stderr, '', "invalid: No standard error output";
};

##########################################################################

subtest "clockd: missing certificate" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/clockd.conf", <<EOF;
security.cert /path/to/nowhere
EOF
	my ($stdout, $stderr);
	local $ENV{TEST_CLOCKD_BAIL_EARLY} = 1;
	run_ok "./TEST_clockd -Fvc t/tmp/clockd.conf", 1, \$stdout, \$stderr,
		"Run clockd with an invalid certificate";

	$stdout =~ s/^clockd\[\d+\]\s*//mg;
	string_is $stdout, <<EOF, "nocert: Standard output";
/path/to/nowhere: No such file or directory
EOF

	string_is $stderr, '', "nocert: No standard error output";
};

##########################################################################

subtest "clockd: missing private key component of certificate" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/clockd.conf", <<EOF;
security.cert t/tmp/cert
EOF
	put_file "t/tmp/cert", <<EOF;
id  pubonly.test
pub aea625f90ac478b9bc9649f6ab74e5f095bab8522f107b1a6d701e83c41cfc4b
EOF
	my ($stdout, $stderr);
	local $ENV{TEST_CLOCKD_BAIL_EARLY} = 1;
	run_ok "./TEST_clockd -Fvc t/tmp/clockd.conf", 1, \$stdout, \$stderr,
		"Run clockd with pub-only certificate";

	$stdout =~ s/^clockd\[\d+\]\s*//mg;
	string_is $stdout, <<EOF, "pubonly: Standard output";
t/tmp/cert: No secret key found in certificate
EOF

	string_is $stderr, '', "pubonly: No standard error output";
};

##########################################################################

subtest "clockd: anonymous certificate" => sub {
	mkdir "t/tmp";
	put_file "t/tmp/clockd.conf", <<EOF;
security.cert t/tmp/cert
EOF
	put_file "t/tmp/cert", <<EOF;
pub aea625f90ac478b9bc9649f6ab74e5f095bab8522f107b1a6d701e83c41cfc4b
sec aea625f90ac478b9bc9649f6ab74e5f095bab8522f107b1a6d701e83c41cfc4b
EOF
	my ($stdout, $stderr);
	local $ENV{TEST_CLOCKD_BAIL_EARLY} = 1;
	run_ok "./TEST_clockd -Fvc t/tmp/clockd.conf", 1, \$stdout, \$stderr,
		"Run clockd with anonymous certificate";

	$stdout =~ s/^clockd\[\d+\]\s*//mg;
	string_is $stdout, <<EOF, "noident: Standard output";
t/tmp/cert: No identity found in certificate
EOF

	string_is $stderr, '', "noident: No standard error output";
};

##########################################################################

subtest "strict verification failure" => sub {
	mkdir $_ for qw(t/tmp t/tmp/gather.d);
	put_file "t/tmp/clockd.conf", <<EOF;
listen *:2313
security.cert    t/tmp/master
security.trusted t/tmp/trusted
security.strict  yes
copydown t/tmp/copydown
manifest t/tmp/manifest.pol
EOF
	put_file "t/tmp/master", <<EOF;
id  master.host.fq.dn
pub d3043ed5285b3cfbaaf3dace0ee9d68c7e3e01d4dc6aa092d90a55b7dfd4fb57
sec 39594d56e97c363dd179205f64112339533500740706c160f4c0ab1303f97328
EOF
	put_file "t/tmp/trusted", <<EOF;
bd60ed6d2daabaaa3d120c1c3269b40df6f8fa8219c538d7ee3f5d47c87abf66 client.host.fq.dn
EOF
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:2313
cert.1   t/tmp/altmaster.pub

security.cert t/tmp/client
lockdir       t/tmp
copydown      t/tmp/local
gatherers     t/tmp/gather.d/*
EOF
	put_file "t/tmp/client", <<EOF;
id  client.host.fq.dn
pub bd60ed6d2daabaaa3d120c1c3269b40df6f8fa8219c538d7ee3f5d47c87abf66
sec e9e78788351c307705fa25eb318349fb77cdd6be5299b365ca27a6c6044761ee
EOF
	put_file "t/tmp/altmaster.pub", <<EOF;
id  altmaster.fq.dn
pub e083e126f5edfbce563906892552f8f0b11ab47bb62f3b23f1bfcb2b650d6f7e
EOF
	put_file "t/tmp/gather.d/core", <<EOF;
#/bin/sh
echo "test=yes"
EOF
	chmod 0755, "t/tmp/gather.d/core";

	my $pid = background_ok "./TEST_clockd -Fc t/tmp/clockd.conf",
		"run clockd in the background";
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Xc t/tmp/cogd.conf", 0, \$stdout, \$stderr,
		"run cogd that isn't trusted by clockd in security.strict = yes";
	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	string_is $stderr, <<EOF, "Standard error output";
Starting configuration run
No response from master 1 (127.0.0.1:2313): possible certificate mismatch
No masters were reachable; giving up
EOF
	kill 9, $pid;
};

##########################################################################

subtest "strict verification success" => sub {
	mkdir $_ for qw(t/tmp t/tmp/copydown t/tmp/gather.d);
	put_file "t/tmp/clockd.conf", <<EOF;
listen *:2313
security.cert    t/tmp/master
security.trusted t/tmp/trusted
security.strict  yes
copydown t/tmp/copydown
manifest t/tmp/manifest.pol
EOF
	put_file "t/tmp/manifest.pol", <<EOF;
policy "base" { }
host fallback { enforce "base" }
EOF
	put_file "t/tmp/master", <<EOF;
id  master.host.fq.dn
pub d3043ed5285b3cfbaaf3dace0ee9d68c7e3e01d4dc6aa092d90a55b7dfd4fb57
sec 39594d56e97c363dd179205f64112339533500740706c160f4c0ab1303f97328
EOF
	put_file "t/tmp/master.pub", <<EOF;
id  master.host.fq.dn
pub d3043ed5285b3cfbaaf3dace0ee9d68c7e3e01d4dc6aa092d90a55b7dfd4fb57
EOF
	put_file "t/tmp/trusted", <<EOF;
bd60ed6d2daabaaa3d120c1c3269b40df6f8fa8219c538d7ee3f5d47c87abf66 client.host.fq.dn
EOF
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:2313
cert.1   t/tmp/master.pub
timeout  90

security.cert t/tmp/client
lockdir       t/tmp
copydown      t/tmp/local
gatherers     t/tmp/gather.d/*
EOF
	put_file "t/tmp/client", <<EOF;
id  client.host.fq.dn
pub bd60ed6d2daabaaa3d120c1c3269b40df6f8fa8219c538d7ee3f5d47c87abf66
sec e9e78788351c307705fa25eb318349fb77cdd6be5299b365ca27a6c6044761ee
EOF
	put_file "t/tmp/gather.d/core", <<EOF;
#/bin/sh
echo "test=yes"
EOF
	chmod 0755, "t/tmp/gather.d/core";

	my $pid = background_ok "./TEST_clockd -Fc t/tmp/clockd.conf",
		"run clockd in the background";
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Xc t/tmp/cogd.conf", 0, \$stdout, \$stderr,
		"run cogd that is trusted by clockd in security.strict = yes";
	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	$stderr =~ s/=\d+/=X/g;
	$stderr =~ s/in \d\.\d+s/in #.##s/;
	string_is $stderr, <<EOF, "Standard error output";
Starting configuration run
66 6e [20 6d 61 69]
      [6e 0a 00 00]
complete. enforced 0 resources in #.##s
STATS(ms): connect=X, hello=X, preinit=X, copydown=X, facts=X, getpolicy=X, parse=X, enforce=X, cleanup=X
EOF
	kill 9, $pid;
};

##########################################################################

subtest "non-strict verification failure" => sub {
	mkdir $_ for qw(t/tmp t/tmp/copydown t/tmp/gather.d);
	put_file "t/tmp/clockd.conf", <<EOF;
listen *:2313
security.cert    t/tmp/master
security.strict  no
copydown t/tmp/copydown
manifest t/tmp/manifest.pol
EOF
	put_file "t/tmp/master", <<EOF;
id  master.host.fq.dn
pub d3043ed5285b3cfbaaf3dace0ee9d68c7e3e01d4dc6aa092d90a55b7dfd4fb57
sec 39594d56e97c363dd179205f64112339533500740706c160f4c0ab1303f97328
EOF
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:2313
cert.1   t/tmp/altmaster.pub

security.cert t/tmp/client
lockdir       t/tmp
copydown      t/tmp/local
gatherers     t/tmp/gather.d/*
EOF
	put_file "t/tmp/client", <<EOF;
id  client.host.fq.dn
pub bd60ed6d2daabaaa3d120c1c3269b40df6f8fa8219c538d7ee3f5d47c87abf66
sec e9e78788351c307705fa25eb318349fb77cdd6be5299b365ca27a6c6044761ee
EOF
	put_file "t/tmp/altmaster.pub", <<EOF;
id  altmaster.fq.dn
pub e083e126f5edfbce563906892552f8f0b11ab47bb62f3b23f1bfcb2b650d6f7e
EOF
	put_file "t/tmp/gather.d/core", <<EOF;
#/bin/sh
echo "test=yes"
EOF
	chmod 0755, "t/tmp/gather.d/core";

	my $pid = background_ok "./TEST_clockd -Fc t/tmp/clockd.conf",
		"run clockd in the background";
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Xc t/tmp/cogd.conf", 0, \$stdout, \$stderr,
		"run cogd that isn't trusted by clockd in security.strict = yes";
	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	string_is $stderr, <<EOF, "Standard error output";
Starting configuration run
No response from master 1 (127.0.0.1:2313): possible certificate mismatch
No masters were reachable; giving up
EOF
	kill 9, $pid;
};

##########################################################################

subtest "non-strict verification success" => sub {
	mkdir $_ for qw(t/tmp t/tmp/copydown t/tmp/gather.d);
	put_file "t/tmp/clockd.conf", <<EOF;
listen *:2313
security.cert    t/tmp/master
security.strict  no
copydown t/tmp/copydown
manifest t/tmp/manifest.pol
EOF
	put_file "t/tmp/master", <<EOF;
id  master.host.fq.dn
pub d3043ed5285b3cfbaaf3dace0ee9d68c7e3e01d4dc6aa092d90a55b7dfd4fb57
sec 39594d56e97c363dd179205f64112339533500740706c160f4c0ab1303f97328
EOF
	put_file "t/tmp/master.pub", <<EOF;
id  master.host.fq.dn
pub d3043ed5285b3cfbaaf3dace0ee9d68c7e3e01d4dc6aa092d90a55b7dfd4fb57
EOF
	put_file "t/tmp/cogd.conf", <<EOF;
master.1 127.0.0.1:2313
cert.1   t/tmp/master.pub

security.cert t/tmp/client
lockdir       t/tmp
copydown      t/tmp/local
gatherers     t/tmp/gather.d/*
EOF
	put_file "t/tmp/client", <<EOF;
id  client.host.fq.dn
pub bd60ed6d2daabaaa3d120c1c3269b40df6f8fa8219c538d7ee3f5d47c87abf66
sec e9e78788351c307705fa25eb318349fb77cdd6be5299b365ca27a6c6044761ee
EOF
	put_file "t/tmp/gather.d/core", <<EOF;
#/bin/sh
echo "test=yes"
EOF
	chmod 0755, "t/tmp/gather.d/core";

	my $pid = background_ok "./TEST_clockd -Fc t/tmp/clockd.conf",
		"run clockd in the background";
	my ($stdout, $stderr);
	run_ok "./TEST_cogd -Xc t/tmp/cogd.conf", 0, \$stdout, \$stderr,
		"run cogd that is trusted by clockd in security.strict = no";
	$stderr =~ s/^cogd\[\d+\]\s*//mg;
	$stderr =~ s/=\d+/=X/g;
	$stderr =~ s/in \d\.\d+s/in #.##s/;
	string_is $stderr, <<EOF, "Standard error output";
Starting configuration run
66 6e [20 6d 61 69]
      [6e 0a 00 00]
complete. enforced 0 resources in #.##s
STATS(ms): connect=X, hello=X, preinit=X, copydown=X, facts=X, getpolicy=X, parse=X, enforce=X, cleanup=X
EOF
	kill 9, $pid;
};

##########################################################################

subtest "trustdb management via cw-trust" => sub {
	unlink "t/tmp/trustdb";
	my ($stdout, $stderr);

	put_file "t/tmp/test1.host", <<EOF;
id  test1.host
pub 132638e39475fdae2765c43436fa03a4581ef9ccdb4b73cad5db41be67510f0d
EOF
	put_file "t/tmp/test2.host", <<EOF;
id  test2.host
pub ddc0bcdaf1a7847a8364162c429e7c3ad6361970ef5166e1cede1536525ab56f
EOF
	put_file "t/tmp/test3.host", <<EOF;
id  test3.host
pub e0c84bfd7db975343faeca05ea6b4427fbc028ed926d9c9b600c1523b6f01238
sec 95df16500db877fc08ce111e15632d2e10e643a268c0bc88b95478d548d6debc
EOF

	run_ok "./cw-trust --database t/tmp/trustdb --trust t/tmp/test1.host", 0, \$stdout, \$stderr,
		"Trust test1.host cert";
	string_is $stdout, <<EOF, "standard output from trusting test1.host";
TRUST 132638e39475fdae2765c43436fa03a4581ef9ccdb4b73cad5db41be67510f0d test1.host
Processed 1 certificate
Wrote t/tmp/trustdb
EOF
	string_is $stderr, '', "no standard error";
	file_is "t/tmp/trustdb", <<EOF, "wrote t/tmp/trustdb after trusting test1.host";
132638e39475fdae2765c43436fa03a4581ef9ccdb4b73cad5db41be67510f0d test1.host
EOF

	run_ok "./cw-trust -d t/tmp/trustdb -t t/tmp/test2.host t/tmp/test3.host", 0, \$stdout, \$stderr,
		"Trust test2.host and test3.host (both keys)";
	string_is $stdout, <<EOF, "standard output from multi-trust op";
TRUST ddc0bcdaf1a7847a8364162c429e7c3ad6361970ef5166e1cede1536525ab56f test2.host
TRUST e0c84bfd7db975343faeca05ea6b4427fbc028ed926d9c9b600c1523b6f01238 test3.host
Processed 2 certificates
Wrote t/tmp/trustdb
EOF
	string_is $stderr, '', "no standard error";
	file_is "t/tmp/trustdb", <<EOF, "wrote t/tmp/trustdb after multi-trust op";
e0c84bfd7db975343faeca05ea6b4427fbc028ed926d9c9b600c1523b6f01238 test3.host
ddc0bcdaf1a7847a8364162c429e7c3ad6361970ef5166e1cede1536525ab56f test2.host
132638e39475fdae2765c43436fa03a4581ef9ccdb4b73cad5db41be67510f0d test1.host
EOF

	run_ok "./cw-trust -d t/tmp/trustdb -r t/tmp/test2.host", 0, \$stdout, \$stderr,
		"Revoke test2.host";
	string_is $stdout, <<EOF, "standard output from revoke op";
REVOKE ddc0bcdaf1a7847a8364162c429e7c3ad6361970ef5166e1cede1536525ab56f test2.host
Processed 1 certificate
Wrote t/tmp/trustdb
EOF
	string_is $stderr, '', "no standard error";
	file_is "t/tmp/trustdb", <<EOF, "wrote t/tmp/trustdb after revoking test2.host";
132638e39475fdae2765c43436fa03a4581ef9ccdb4b73cad5db41be67510f0d test1.host
e0c84bfd7db975343faeca05ea6b4427fbc028ed926d9c9b600c1523b6f01238 test3.host
EOF
};

##########################################################################

done_testing;
