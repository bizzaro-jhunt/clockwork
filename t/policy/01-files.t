#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host file1.test", <<'EOF', "file resource";
RESET
TOPIC "file(/etc/sudoers)"
SET %A "/etc/sudoers"
CALL &FS.EXISTS?
OK? @exists.1
  CALL &FS.MKFILE
  OK? @exists.1
  ERROR "Failed to create new file '%s'"
  JUMP @next.1
exists.1:
CALL &FS.IS_FILE?
OK? @isfile.1
  ERROR "%s exists, but is not a regular file"
  JUMP @next.1
isfile.1:
CALL &USERDB.OPEN
OK? @start.1
  ERROR "Failed to open the user database"
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
  ERROR "Failed to find user '%s'"
  JUMP @next.1
found.user.1:
CALL &USER.GET_UID
COPY %R %D
SET %A 1
SET %B "root"
CALL &GROUP.FIND
OK? @found.group.1
  COPY %B %A
  ERROR "Failed to find group '%s'"
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
RESET
TOPIC "file(/path/to/delete)"
SET %A "/path/to/delete"
CALL &FS.EXISTS?
NOTOK? @next.1
CALL &FS.IS_FILE?
OK? @isfile.1
  ERROR "%s exists, but is not a regular file"
  JUMP @next.1
isfile.1:
CALL &FS.UNLINK
JUMP @next.1
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host file3.test", <<'EOF', "file without chown";
RESET
TOPIC "file(/chmod-me)"
SET %A "/chmod-me"
CALL &FS.EXISTS?
OK? @exists.1
  CALL &FS.MKFILE
  OK? @exists.1
  ERROR "Failed to create new file '%s'"
  JUMP @next.1
exists.1:
CALL &FS.IS_FILE?
OK? @isfile.1
  ERROR "%s exists, but is not a regular file"
  JUMP @next.1
isfile.1:
SET %D 0644
CALL &FS.CHMOD
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host file4.test", <<'EOF', "file with non-root owner";
RESET
TOPIC "file(/home/jrhunt/stuff)"
SET %A "/home/jrhunt/stuff"
CALL &FS.EXISTS?
OK? @exists.1
  CALL &FS.MKFILE
  OK? @exists.1
  ERROR "Failed to create new file '%s'"
  JUMP @next.1
exists.1:
CALL &FS.IS_FILE?
OK? @isfile.1
  ERROR "%s exists, but is not a regular file"
  JUMP @next.1
isfile.1:
CALL &USERDB.OPEN
OK? @start.1
  ERROR "Failed to open the user database"
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
  ERROR "Failed to find user '%s'"
  JUMP @next.1
found.user.1:
CALL &USER.GET_UID
COPY %R %D
SET %A 1
SET %B "staff"
CALL &GROUP.FIND
OK? @found.group.1
  COPY %B %A
  ERROR "Failed to find group '%s'"
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

gencode_ok "use host file5.test", <<'EOF', "file contents update";
RESET
TOPIC "file(/etc/file.conf)"
SET %A "/etc/file.conf"
CALL &FS.EXISTS?
OK? @exists.1
  CALL &FS.MKFILE
  OK? @exists.1
  ERROR "Failed to create new file '%s'"
  JUMP @next.1
exists.1:
CALL &FS.IS_FILE?
OK? @isfile.1
  ERROR "%s exists, but is not a regular file"
  JUMP @next.1
isfile.1:
SET %D 0644
CALL &FS.CHMOD
CALL &FS.SHA1
OK? @localok.1
  ERROR "Failed to calculate SHA1 for local copy of '%s'"
  JUMP @sha1.done.1
localok.1:
COPY %S2 %T1
COPY %A %F
SET %A "file:/etc/file.conf"
;; source = /cfm/files/path/to//etc/file.conf
CALL &SERVER.SHA1
COPY %F %A
OK? @remoteok.1
  ERROR "Failed to retrieve SHA1 of expected contents"
  JUMP @sha1.done.1
remoteok.1:
COPY %S2 %T2
CMP? @sha1.done.1
  COPY %T1 %A
  COPY %T2 %B
  LOG NOTICE "Updating local content (%s) from remote copy (%s)"
  COPY %F %A
  CALL &SERVER.WRITEFILE
  OK? @sha1.done.1
    ERROR "Failed to update local file contents"
sha1.done.1:
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

done_testing;
