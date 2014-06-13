#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host package1.test", <<'EOF', "package resource";
RESET
TOPIC "package(binutils)"
SET %A "cwtool pkg-version binutils"
CALL &EXEC.RUN1
OK? @installed.1
  SET %A "cwtool pkg-install binutils latest"
  CALL &EXEC.CHECK
  FLAG 1 :changed
  JUMP @next.1
installed.1:
COPY %S2 %T1
SET %A "cwtool pkg-latest binutils"
CALL &EXEC.RUN1
OK? @got.latest.1
  ERROR "Failed to detect latest version of 'binutils'"
  JUMP @next.1
got.latest.1:
COPY %S2 %T2
CALL &UTIL.VERCMP
OK? @next.1
  SET %A "cwtool pkg-install binutils latest"
  CALL &EXEC.CHECK
  FLAG 1 :changed
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host package2.test", <<'EOF', "package uninstall";
RESET
TOPIC "package(binutils)"
SET %A "cwtool pkg-version binutils"
CALL &EXEC.RUN1
NOTOK? @next.1
  SET %A "cwtool pkg-remove binutils"
  CALL &EXEC.CHECK
  FLAG 1 :changed
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host package3.test", <<'EOF', "package versioned install";
RESET
TOPIC "package(binutils)"
SET %A "cwtool pkg-version binutils"
CALL &EXEC.RUN1
OK? @installed.1
  SET %A "cwtool pkg-install binutils 1.2.3"
  CALL &EXEC.CHECK
  FLAG 1 :changed
  JUMP @next.1
installed.1:
COPY %S2 %T1
SET %T2 "1.2.3"
CALL &UTIL.VERCMP
OK? @next.1
  SET %A "cwtool pkg-install binutils 1.2.3"
  CALL &EXEC.CHECK
  FLAG 1 :changed
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF


done_testing;
