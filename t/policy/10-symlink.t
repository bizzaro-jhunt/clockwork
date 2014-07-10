#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host symlink1.test", <<'EOF', "symlink removal";
RESET
TOPIC "symlink(/etc/old/symlink)"
SET %A "/etc/old/symlink"
SET %B "/dev/null"
CALL &FS.EXISTS?
NOTOK? @next.1
CALL &FS.IS_SYMLINK?
OK? @islink.1
  ERROR "%s exists, but is not a symlink\n"
  JUMP @next.1
islink.1:
CALL &FS.UNLINK
JUMP @next.1
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host symlink2.test", <<'EOF', "symlink";
RESET
TOPIC "symlink(/etc/old/symlink)"
SET %A "/etc/old/symlink"
SET %B "/a/target/file"
CALL &FS.EXISTS?
NOTOK? @create.1
  CALL &FS.IS_SYMLINK?
  OK? @islink.1
    ERROR "%s exists, but is not a symlink\n"
    JUMP @next.1
  islink.1:
  CALL &FS.UNLINK
CALL &FS.EXISTS?
NOTOK? @create.1
  ERROR "Failed to unlink %s\n"
  JUMP @next.1
create.1:
CALL &FS.SYMLINK
OK? @next.1
  ERROR "Failed to symlink %s -> %s\n"
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host symlink3.test", <<'EOF', "missing target";
RESET
TOPIC "symlink(/etc/old/symlink)"
SET %A "/etc/old/symlink"
SET %B "/dev/null"
CALL &FS.EXISTS?
NOTOK? @create.1
  CALL &FS.IS_SYMLINK?
  OK? @islink.1
    ERROR "%s exists, but is not a symlink\n"
    JUMP @next.1
  islink.1:
  CALL &FS.UNLINK
CALL &FS.EXISTS?
NOTOK? @create.1
  ERROR "Failed to unlink %s\n"
  JUMP @next.1
create.1:
CALL &FS.SYMLINK
OK? @next.1
  ERROR "Failed to symlink %s -> %s\n"
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

done_testing;
