#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host deps1.test", <<'EOF', "implict deps";
FLAG 0 :changed
;; res_dir /tmp
SET %A "/tmp"
CALL &FS.MKDIR
next.1:
!FLAGGED? :changed @final.1
  FLAG 1 :res2
  FLAG 1 :res3
final.1:
FLAG 0 :changed
;; res_dir /tmp/inner
SET %A "/tmp/inner"
CALL &FS.MKDIR
next.2:
!FLAGGED? :changed @final.2
  FLAG 1 :res2
final.2:
FLAG 0 :changed
;; res_file /tmp/inner/file
SET %A "/tmp/inner/file"
CALL &FS.MKFILE
next.3:
!FLAGGED? :changed @final.3
final.3:
EOF



done_testing;
