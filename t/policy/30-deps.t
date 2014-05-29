#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host deps1.test", <<'EOF', "implict deps";
FLAG 0 :changed
;; res_dir /tmp
SET %A "/tmp"
CALL &FS.EXISTS?
OK? @exists.1
  CALL &FS.MKDIR
  FLAG 1 :changed
exists.1:
next.1:
!FLAGGED? :changed @final.1
  FLAG 1 :res2
  FLAG 1 :res3
final.1:
FLAG 0 :changed
;; res_dir /tmp/inner
SET %A "/tmp/inner"
CALL &FS.EXISTS?
OK? @exists.2
  CALL &FS.MKDIR
  FLAG 1 :changed
exists.2:
next.2:
!FLAGGED? :changed @final.2
  FLAG 1 :res2
final.2:
FLAG 0 :changed
;; res_file /tmp/inner/file
SET %A "/tmp/inner/file"
CALL &FS.EXISTS?
OK? @exists.3
  CALL &FS.MKFILE
  FLAG 1 :changed
exists.3:
next.3:
!FLAGGED? :changed @final.3
final.3:
EOF



done_testing;
