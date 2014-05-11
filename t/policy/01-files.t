#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::policy::common;

gencode_ok "use host file1.test", <<'EOF', "file resource";
;; res_file /etc/sudoers
SET %A "/etc/sudoers"
CALL &FS.EXISTS?
OK? @create.1
  JUMP @exists.1
create.1:
  CALL &FS.MKFILE
exists.1:
COPY %A %F
SET %D 0
SET %E 0
SET %A 1
SET %B "root"
CALL &USER.FIND
NOTOK? @found.user.1
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
NOTOK? @found.group.1
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
EOF

gencode_ok "use host file2.test", <<'EOF', "file removal";
;; res_file /path/to/delete
SET %A "/path/to/delete"
CALL &FS.EXISTS?
OK? @next.1
  CALL &FS.UNLINK
  JUMP @next.1
next.1:
EOF

gencode_ok "use host file3.test", <<'EOF', "file without chown";
;; res_file /chmod-me
SET %A "/chmod-me"
CALL &FS.EXISTS?
OK? @create.1
  JUMP @exists.1
create.1:
  CALL &FS.MKFILE
exists.1:
SET %B 0644
CALL &FS.CHMOD
next.1:
EOF

gencode_ok "use host file4.test", <<'EOF', "file with non-root owner";
;; res_file /home/jrhunt/stuff
SET %A "/home/jrhunt/stuff"
CALL &FS.EXISTS?
OK? @create.1
  JUMP @exists.1
create.1:
  CALL &FS.MKFILE
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
SET %B 0410
CALL &FS.CHMOD
next.1:
EOF

done_testing;
