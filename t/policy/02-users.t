#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host user1.test", <<'EOF', "user removal";
FLAG 0 :changed
;; res_user t1user
CALL &USERDB.OPEN
NOTOK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
SET %A 1
SET %B "t1user"
CALL &USER.FIND
OK? @next.1
  CALL &USER.REMOVE
  FLAG 1 :changed
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host user2.test", <<'EOF', "user creation with explicit UID/GID";
FLAG 0 :changed
;; res_user t2user
CALL &USERDB.OPEN
NOTOK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
SET %A 1
SET %B "t2user"
CALL &USER.FIND
OK? @create.1
  JUMP @check.ids.1
create.1:
SET %C 1818
SET %B 1231
CALL &USER.CREATE
FLAG 1 :changed
SET %B "x"
CALL &USER.SET_PASSWD
SET %B "$$crypto"
COPY %B %T2
CALL &USER.GET_PWHASH
COPY %R %T1
DIFF? @pwhash.ok.1
  CALL &USER.SET_PWHASH
  FLAG 1 :changed
pwhash.ok.1:
JUMP @exists.1
check.ids.1:
SET %B 1231
COPY %B %T2
CALL &USER.GET_UID
COPY %R %T1
EQ? @uid.ok.1
  CALL &USER.SET_UID
  FLAG 1 :changed
uid.ok.1:
SET %B 1818
COPY %B %T2
CALL &USER.GET_GID
COPY %R %T1
NE? @gid.ok.1
  CALL &USER.SET_GID
  FLAG 1 :changed
gid.ok.1:
SET %B "/home/t2user"
COPY %B %T1
CALL &USER.GET_HOME
COPY %R %T2
DIFF? @home.ok.1
  CALL &USER.SET_HOME
  FLAG 1 :changed
home.ok.1:
exists.1:
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host user3.test", <<'EOF', "user creation without UID/GID";
FLAG 0 :changed
;; res_user t3user
CALL &USERDB.OPEN
NOTOK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
SET %A 1
SET %B "t3user"
CALL &USER.FIND
OK? @create.1
  JUMP @check.ids.1
create.1:
SET %A 1
SET %B "t3user"
CALL &GROUP.FIND
OK? @group.found.1
  COPY %B %A
  CALL &GROUP.NEXT_GID
  COPY %R %B
  COPY %R %C
  CALL &GROUP.CREATE
  JUMP @group.done.1
group.found.1:
  CALL &GROUP.GET_GID
  COPY %R %C
group.done.1:
CALL &USER.NEXT_UID
COPY %R %B
CALL &USER.CREATE
FLAG 1 :changed
SET %B "x"
CALL &USER.SET_PASSWD
SET %B "$$crypto"
COPY %B %T2
CALL &USER.GET_PWHASH
COPY %R %T1
DIFF? @pwhash.ok.1
  CALL &USER.SET_PWHASH
  FLAG 1 :changed
pwhash.ok.1:
JUMP @exists.1
check.ids.1:
SET %B "/home/t3user"
COPY %B %T1
CALL &USER.GET_HOME
COPY %R %T2
DIFF? @home.ok.1
  CALL &USER.SET_HOME
  FLAG 1 :changed
home.ok.1:
CALL &USER.GET_UID
COPY %R %B
CALL &USER.GET_GID
COPY %R %C
CALL &USER.GET_HOME
COPY %R %A
CALL &FS.EXISTS?
NOTOK? @home.exists.1
  CALL &FS.MKDIR
  CALL &FS.CHOWN
  SET %D 448
  CALL &FS.CHMOD
  COPY %C %D
  COPY %B %C
  COPY %A %B
  SET %A "/etc/skel"
  CALL &FS.COPY_R
home.exists.1:
exists.1:
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host user4.test", <<'EOF', "user with all attrs";
FLAG 0 :changed
;; res_user t4user
CALL &USERDB.OPEN
NOTOK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
SET %A 1
SET %B "t4user"
CALL &USER.FIND
OK? @create.1
  JUMP @check.ids.1
create.1:
SET %C 1818
SET %B 1231
CALL &USER.CREATE
FLAG 1 :changed
JUMP @exists.1
check.ids.1:
SET %B 1231
COPY %B %T2
CALL &USER.GET_UID
COPY %R %T1
EQ? @uid.ok.1
  CALL &USER.SET_UID
  FLAG 1 :changed
uid.ok.1:
SET %B 1818
COPY %B %T2
CALL &USER.GET_GID
COPY %R %T1
NE? @gid.ok.1
  CALL &USER.SET_GID
  FLAG 1 :changed
gid.ok.1:
SET %B "/home/t4user"
COPY %B %T1
CALL &USER.GET_HOME
COPY %R %T2
DIFF? @home.ok.1
  CALL &USER.SET_HOME
  FLAG 1 :changed
home.ok.1:
exists.1:
SET %B "x"
CALL &USER.SET_PASSWD
SET %B "$$crypto"
COPY %B %T2
CALL &USER.GET_PWHASH
COPY %R %T1
DIFF? @pwhash.ok.1
  CALL &USER.SET_PWHASH
  FLAG 1 :changed
pwhash.ok.1:
SET %B "Name,,,,"
COPY %B %T2
CALL &USER.GET_GECOS
COPY %R %T1
DIFF? @gecos.ok.1
  CALL &USER.SET_GECOS
  FLAG 1 :changed
gecos.ok.1:
SET %B "/bin/bash"
COPY %B %T2
CALL &USER.GET_SHELL
COPY %R %T1
DIFF? @shell.ok.1
  CALL &USER.SET_SHELL
  FLAG 1 :changed
shell.ok.1:
SET %B 99
COPY %B %T2
CALL &USER.GET_PWMIN
COPY %R %T1
NE? @pwmin.ok.1
  CALL &USER.SET_PWMIN
  FLAG 1 :changed
pwmin.ok.1:
SET %B 305
COPY %B %T2
CALL &USER.GET_PWMAX
COPY %R %T1
NE? @pwmax.ok.1
  CALL &USER.SET_PWMAX
  FLAG 1 :changed
pwmax.ok.1:
SET %B 14
COPY %B %T2
CALL &USER.GET_PWWARN
COPY %R %T1
NE? @pwwarn.ok.1
  CALL &USER.SET_PWWARN
  FLAG 1 :changed
pwwarn.ok.1:
SET %B 9998
COPY %B %T2
CALL &USER.GET_INACT
COPY %R %T1
NE? @inact.ok.1
  CALL &USER.SET_INACT
  FLAG 1 :changed
inact.ok.1:
SET %B 9999
COPY %B %T2
CALL &USER.GET_EXPIRY
COPY %R %T1
NE? @expire.ok.1
  CALL &USER.SET_EXPIRY
  FLAG 1 :changed
expire.ok.1:
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

done_testing;
