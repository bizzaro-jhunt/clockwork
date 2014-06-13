#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host user1.test", <<'EOF', "user removal";
RESET
TOPIC "user(t1user)"
CALL &USERDB.OPEN
OK? @start.1
  ERROR "Failed to open the user database"
  HALT
start.1:
SET %A 1
SET %B "t1user"
CALL &USER.FIND
NOTOK? @next.1
  CALL &USER.REMOVE
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host user2.test", <<'EOF', "user creation with explicit UID/GID";
RESET
TOPIC "user(t2user)"
CALL &USERDB.OPEN
OK? @start.1
  ERROR "Failed to open the user database"
  HALT
start.1:
SET %A 1
SET %B "t2user"
CALL &USER.FIND
OK? @check.ids.1
  SET %C 1818
  SET %B 1231
  SET %A "t2user"
  CALL &USER.CREATE
  SET %B "x"
  CALL &USER.SET_PASSWD
  SET %B "$$crypto"
  CALL &USER.SET_PWHASH
  JUMP @exists.1
check.ids.1:
SET %B 1231
CALL &USER.SET_UID
SET %B 1818
CALL &USER.SET_GID
exists.1:
SET %B "/home/t2user"
CALL &USER.SET_HOME
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host user3.test", <<'EOF', "user creation without UID/GID";
RESET
TOPIC "user(t3user)"
CALL &USERDB.OPEN
OK? @start.1
  ERROR "Failed to open the user database"
  HALT
start.1:
SET %A 1
SET %B "t3user"
CALL &USER.FIND
OK? @check.ids.1
  SET %A 1
  SET %B "t3user"
  CALL &GROUP.FIND
  OK? @group.found.1
    COPY %B %A
    ERROR "Failed to find group '%s'"
    JUMP @next.1
  group.found.1:
    CALL &GROUP.GET_GID
    COPY %R %C
group.done.1:
  CALL &USER.NEXT_UID
  COPY %R %B
  SET %A "t3user"
  CALL &USER.CREATE
  SET %B "x"
  CALL &USER.SET_PASSWD
  SET %B "$$crypto"
  CALL &USER.SET_PWHASH
  FLAG 1 :mkhome.1
  JUMP @exists.1
check.ids.1:
exists.1:
SET %B "/home/t3user"
CALL &USER.SET_HOME
!FLAGGED? :mkhome.1 @home.exists.1
  CALL &USER.GET_UID
  COPY %R %B
  CALL &USER.GET_GID
  COPY %R %C
  CALL &USER.GET_HOME
  COPY %R %A
  CALL &FS.EXISTS?
  OK? @home.exists.1
    CALL &FS.MKDIR
    CALL &FS.CHOWN
    SET %D 0700
    CALL &FS.CHMOD
    COPY %C %D
    COPY %B %C
    COPY %A %B
    SET %A "/etc/skel"
    CALL &FS.COPY_R
home.exists.1:
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host user4.test", <<'EOF', "user with all attrs";
RESET
TOPIC "user(t4user)"
CALL &USERDB.OPEN
OK? @start.1
  ERROR "Failed to open the user database"
  HALT
start.1:
SET %A 1
SET %B "t4user"
CALL &USER.FIND
OK? @check.ids.1
  SET %C 1818
  SET %B 1231
  SET %A "t4user"
  CALL &USER.CREATE
  SET %B "x"
  CALL &USER.SET_PASSWD
  SET %B "*"
  CALL &USER.SET_PWHASH
  JUMP @exists.1
check.ids.1:
SET %B 1231
CALL &USER.SET_UID
SET %B 1818
CALL &USER.SET_GID
exists.1:
SET %B "x"
CALL &USER.SET_PASSWD
SET %B "$$crypto"
CALL &USER.SET_PWHASH
SET %B "/home/t4user"
CALL &USER.SET_HOME
SET %B "Name,,,,"
CALL &USER.SET_GECOS
SET %B "/bin/bash"
CALL &USER.SET_SHELL
SET %B 99
CALL &USER.SET_PWMIN
SET %B 305
CALL &USER.SET_PWMAX
SET %B 14
CALL &USER.SET_PWWARN
SET %B 9998
CALL &USER.SET_INACT
SET %B 9999
CALL &USER.SET_EXPIRY
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

done_testing;
