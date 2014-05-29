#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host file1.test", <<'EOF', "file resource";
FLAG 0 :changed
;; res_file /etc/sudoers
SET %A "/etc/sudoers"
CALL &FS.EXISTS?
OK? @exists.1
  CALL &FS.MKFILE
  FLAG 1 :changed
exists.1:
CALL &USERDB.OPEN
OK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
COPY %A %F
SET %D 0
SET %E 0
SET %A 1
SET %B "root"
CALL &USER.FIND
OK? @found.user.1
  COPY %B %A
  PRINT "Unable to find user '%s'\n"
  JUMP @next.1
found.user.1:
CALL &USER.GET_UID
COPY %R %D
SET %A 1
SET %B "root"
CALL &GROUP.FIND
OK? @found.group.1
  COPY %B %A
  PRINT "Unable to find group '%s'\n"
  JUMP @next.1
found.group.1:
CALL &GROUP.GET_GID
COPY %R %E
CALL &USERDB.CLOSE
COPY %F %A
COPY %D %B
COPY %E %C
CALL &FS.CHOWN
SET %D 0400
CALL &FS.CHMOD
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host file2.test", <<'EOF', "file removal";
FLAG 0 :changed
;; res_file /path/to/delete
SET %A "/path/to/delete"
CALL &FS.EXISTS?
NOTOK? @next.1
  CALL &FS.UNLINK
  FLAG 1 :changed
  JUMP @next.1
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host file3.test", <<'EOF', "file without chown";
FLAG 0 :changed
;; res_file /chmod-me
SET %A "/chmod-me"
CALL &FS.EXISTS?
OK? @exists.1
  CALL &FS.MKFILE
  FLAG 1 :changed
exists.1:
SET %D 0644
CALL &FS.CHMOD
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host file4.test", <<'EOF', "file with non-root owner";
FLAG 0 :changed
;; res_file /home/jrhunt/stuff
SET %A "/home/jrhunt/stuff"
CALL &FS.EXISTS?
OK? @exists.1
  CALL &FS.MKFILE
  FLAG 1 :changed
exists.1:
CALL &USERDB.OPEN
OK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
COPY %A %F
SET %D 0
SET %E 0
SET %A 1
SET %B "jrhunt"
CALL &USER.FIND
OK? @found.user.1
  COPY %B %A
  PRINT "Unable to find user '%s'\n"
  JUMP @next.1
found.user.1:
CALL &USER.GET_UID
COPY %R %D
SET %A 1
SET %B "staff"
CALL &GROUP.FIND
OK? @found.group.1
  COPY %B %A
  PRINT "Unable to find group '%s'\n"
  JUMP @next.1
found.group.1:
CALL &GROUP.GET_GID
COPY %R %E
CALL &USERDB.CLOSE
COPY %F %A
COPY %D %B
COPY %E %C
CALL &FS.CHOWN
SET %D 0410
CALL &FS.CHMOD
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

done_testing;
