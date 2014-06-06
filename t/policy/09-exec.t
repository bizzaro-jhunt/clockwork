#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host exec1.test", <<'EOF', "exec resource";
RESET
TOPIC "exec(/bin/ls -l /tmp)"
SET %B 0
SET %C 0
SET %A "/bin/ls -l /tmp"
CALL &EXEC.CHECK
OK? @next.1
  FLAG 1 :changed
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host exec2.test", <<'EOF', "ondemand exec";
RESET
TOPIC "exec(/bin/refresh-the-thing)"
SET %B 0
SET %C 0
!FLAGGED? :res0 @next.1
SET %A "/bin/refresh-the-thing"
CALL &EXEC.CHECK
OK? @next.1
  FLAG 1 :changed
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host exec3.test", <<'EOF', "conditional exec";
RESET
TOPIC "exec(CONDITIONAL)"
SET %B 0
SET %C 0
SET %A "/usr/bin/test ! -f /stuff"
CALL &EXEC.CHECK
NOTOK? @next.1
SET %A "/make-stuff"
CALL &EXEC.CHECK
OK? @next.1
  FLAG 1 :changed
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host exec4.test", <<'EOF', "all attrs";
RESET
TOPIC "exec(catmans)"
CALL &USERDB.OPEN
OK? @who.lookup.1
  PRINT "Failed to open the user databases\n"
  HALT
who.lookup.1:
SET %D 0
SET %E 0
SET %A 1
SET %B "librarian"
CALL &USER.FIND
OK? @user.found.1
  PRINT "Failed to find user 'librarian'\n"
  HALT
user.found.1:
CALL &USER.GET_UID
COPY %R %D
SET %A 1
SET %B "booklovers"
CALL &GROUP.FIND
OK? @group.found.1
  PRINT "Failed to find group 'booklovers'\n"
  HALT
group.found.1:
CALL &GROUP.GET_GID
COPY %R %E
COPY %D %B
COPY %E %C
CALL &USERDB.CLOSE
SET %A "/bin/find /usr/share/mans -mtime +1 | grep -q 'xx'"
CALL &EXEC.CHECK
NOTOK? @next.1
SET %A "catmans"
CALL &EXEC.CHECK
OK? @next.1
  FLAG 1 :changed
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF



done_testing;
