#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host host1.test", <<'EOF', "host resource";
RESET
;; res_host example.com
SET %A "/files/etc/hosts/*[ipaddr = \"1.2.3.4\" and canonical = \"example.com\"]"
CALL &AUGEAS.FIND
OK @found.1
  SET %A "/files/etc/hosts/100000/ipaddr"
  SET %B "1.2.3.4"
  CALL &AUGEAS.SET
  SET %A "/files/etc/hosts/100000/canonical"
  SET %B "example.com"
  CALL &AUGEAS.SET
  JUMP @aliases.1
found.1:
  COPY %S2 %A
aliases.1:
SET %C "/alias"
CALL &AUGEAS.REMOVE
SET %C "/alias[0]"
SET %B "www.example.com"
CALL &AUGEAS.SET
SET %C "/alias[1]"
SET %B "example.org"
CALL &AUGEAS.SET
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host host2.test", <<'EOF', "host resource";
RESET
;; res_host remove.me
SET %A "/files/etc/hosts/*[ipaddr = \"2.4.6.8\" and canonical = \"remove.me\"]"
CALL &AUGEAS.FIND
NOTOK @not.found.1
  COPY %R %A
  CALL &AUGEAS.REMOVE
not.found.1:
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF


done_testing;
