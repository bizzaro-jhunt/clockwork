#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host dir1.test", <<'EOF', "dir resource";
FLAG 0 :changed
;; res_dir /etc/sudoers
SET %A "/etc/sudoers"
CALL &FS.EXISTS?
OK? @create.1
  JUMP @exists.1
create.1:
  CALL &FS.MKDIR
exists.1:
COPY %A %F
SET %D 0
SET %E 0
SET %A 1
SET %B "root"
CALL &USER.FIND
OK? @found.user.1
  COPY %B %A
  PRINT "Unable to find user '%s'\n"
  JUMP @userfind.done.1
found.user.1:
CALL &USER.GET_UID
COPY %R %D
userfind.done.1:
SET %A 1
SET %B "root"
CALL &GROUP.FIND
OK? @found.group.1
  COPY %B %A
  PRINT "Unable to find group '%s'\n"
  JUMP @groupfind.done.1
found.group.1:
CALL &GROUP.GET_GID
COPY %R %E
groupfind.done.1:
COPY %F %A
COPY %D %B
COPY %E %C
CALL &FS.CHOWN
SET %B 0400
CALL &FS.CHMOD
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host dir2.test", <<'EOF', "dir removal";
FLAG 0 :changed
;; res_dir /path/to/delete
SET %A "/path/to/delete"
CALL &FS.EXISTS?
OK? @next.1
  CALL &FS.RMDIR
  JUMP @next.1
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host dir3.test", <<'EOF', "dir without chown";
FLAG 0 :changed
;; res_dir /chmod-me
SET %A "/chmod-me"
CALL &FS.EXISTS?
OK? @create.1
  JUMP @exists.1
create.1:
  CALL &FS.MKDIR
exists.1:
SET %B 0755
CALL &FS.CHMOD
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host dir4.test", <<'EOF', "dir with non-root owner";
FLAG 0 :changed
;; res_dir /home/jrhunt/bin
SET %A "/home/jrhunt/bin"
CALL &FS.EXISTS?
OK? @create.1
  JUMP @exists.1
create.1:
  CALL &FS.MKDIR
exists.1:
COPY %A %F
SET %D 0
SET %E 0
SET %A 1
SET %B "jrhunt"
CALL &USER.FIND
OK? @found.user.1
  COPY %B %A
  PRINT "Unable to find user '%s'\n"
  JUMP @userfind.done.1
found.user.1:
CALL &USER.GET_UID
COPY %R %D
userfind.done.1:
SET %A 1
SET %B "staff"
CALL &GROUP.FIND
OK? @found.group.1
  COPY %B %A
  PRINT "Unable to find group '%s'\n"
  JUMP @groupfind.done.1
found.group.1:
CALL &GROUP.GET_GID
COPY %R %E
groupfind.done.1:
COPY %F %A
COPY %D %B
COPY %E %C
CALL &FS.CHOWN
SET %B 0710
CALL &FS.CHMOD
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

done_testing;
