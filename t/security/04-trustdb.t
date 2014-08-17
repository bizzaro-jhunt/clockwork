#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;


my $CERTS = "t/tmp/data/trustdb";
my $TMPDB = "t/tmp/trustdb.1";
unlink $TMPDB;

my ($stdout, $stderr);
run_ok "./cwtrust --database $TMPDB --trust $CERTS/test1.pub", 0, \$stdout, \$stderr,
	"Trust test1.pub cert";
string_is $stdout, <<EOF, "standard output from trusting test1.pub";
TRUST 132638e39475fdae2765c43436fa03a4581ef9ccdb4b73cad5db41be67510f0d test1.host
Processed 1 certificate
Wrote $TMPDB
EOF
string_is $stderr, '', "no standard error";
file_is $TMPDB, <<EOF, "wrote $TMPDB after trusting test1.pub";
132638e39475fdae2765c43436fa03a4581ef9ccdb4b73cad5db41be67510f0d test1.host
EOF

run_ok "./cwtrust -d $TMPDB -t $CERTS/test2.pub $CERTS/test3", 0, \$stdout, \$stderr,
	"Trust test2.pub and test3 (combined pub+sec key)";
string_is $stdout, <<EOF, "standard output from multi-trust op";
TRUST ddc0bcdaf1a7847a8364162c429e7c3ad6361970ef5166e1cede1536525ab56f test2.host
TRUST e0c84bfd7db975343faeca05ea6b4427fbc028ed926d9c9b600c1523b6f01238 test3.host
Processed 2 certificates
Wrote $TMPDB
EOF
string_is $stderr, '', "no standard error";
file_is $TMPDB, <<EOF, "wrote $TMPDB after multi-trust op";
e0c84bfd7db975343faeca05ea6b4427fbc028ed926d9c9b600c1523b6f01238 test3.host
ddc0bcdaf1a7847a8364162c429e7c3ad6361970ef5166e1cede1536525ab56f test2.host
132638e39475fdae2765c43436fa03a4581ef9ccdb4b73cad5db41be67510f0d test1.host
EOF

run_ok "./cwtrust -d $TMPDB -r $CERTS/test2", 0, \$stdout, \$stderr,
	"Revoke test2 (combined pub+sec key)";
string_is $stdout, <<EOF, "standard output from revoke op";
REVOKE ddc0bcdaf1a7847a8364162c429e7c3ad6361970ef5166e1cede1536525ab56f test2.host
Processed 1 certificate
Wrote $TMPDB
EOF
string_is $stderr, '', "no standard error";
file_is $TMPDB, <<EOF, "wrote $TMPDB after revoking test2";
132638e39475fdae2765c43436fa03a4581ef9ccdb4b73cad5db41be67510f0d test1.host
e0c84bfd7db975343faeca05ea6b4427fbc028ed926d9c9b600c1523b6f01238 test3.host
EOF

done_testing;
