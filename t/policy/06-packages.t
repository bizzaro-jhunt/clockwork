#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host package1.test", <<'EOF', "package resource";
FLAG 0 :changed
;; res_package binutils
SET %A "cwtool pkg-install binutils latest"
CALL &EXEC.CHECK
next.1:
FLAGGED? :changed
OK? @final.1
final.1:
EOF

gencode_ok "use host package2.test", <<'EOF', "package uninstall";
FLAG 0 :changed
;; res_package binutils
SET %A "cwtool pkg-remove binutils"
CALL &EXEC.CHECK
next.1:
FLAGGED? :changed
OK? @final.1
final.1:
EOF

gencode_ok "use host package3.test", <<'EOF', "package versioned install";
FLAG 0 :changed
;; res_package binutils
SET %A "cwtool pkg-install binutils 1.2.3"
CALL &EXEC.CHECK
next.1:
FLAGGED? :changed
OK? @final.1
final.1:
EOF


done_testing;
