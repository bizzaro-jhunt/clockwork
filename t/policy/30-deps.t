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
OK? @create.1
  JUMP @exists.1
create.1:
  CALL &FS.MKDIR
exists.1:
next.1:
FLAGGED? :changed
OK? @final.1
  FLAG 1 :res2
  FLAG 1 :res3
final.1:
FLAG 0 :changed
;; res_dir /tmp/inner
SET %A "/tmp/inner"
CALL &FS.EXISTS?
OK? @create.2
  JUMP @exists.2
create.2:
  CALL &FS.MKDIR
exists.2:
next.2:
FLAGGED? :changed
OK? @final.2
  FLAG 1 :res2
final.2:
FLAG 0 :changed
;; res_file /tmp/inner/file
SET %A "/tmp/inner/file"
CALL &FS.EXISTS?
OK? @create.3
  JUMP @exists.3
create.3:
  CALL &FS.MKFILE
exists.3:
next.3:
FLAGGED? :changed
OK? @final.3
final.3:
EOF



done_testing;
