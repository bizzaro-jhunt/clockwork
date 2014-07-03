#!/usr/bin/perl
use strict;
use warnings;

use Test::More;

note "Running unknown.pol";
note qx(./clockd -tvvvvvvvvv -c t/tmp/data/policy/fail/unknown.conf 2>&1);
is $? >> 8, 2, "unknown.pol failed validation";

done_testing;
