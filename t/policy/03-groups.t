#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host group1.test", <<'EOF', "group removal";
FLAG 0 :changed
;; res_group group1
CALL &USERDB.OPEN
OK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
SET %A 1
SET %B "group1"
CALL &GROUP.FIND
NOTOK? @next.1
  CALL &GROUP.REMOVE
  FLAG 1 :changed
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host group2.test", <<'EOF', "group creation with explicit GID";
FLAG 0 :changed
;; res_group group2
CALL &USERDB.OPEN
OK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
SET %A 1
SET %B "group2"
CALL &GROUP.FIND
OK? @found.1
  COPY %B %A
  SET %B 6766
  CALL &GROUP.CREATE
  FLAG 1 :changed
  JUMP @update.1
found.1:
  SET %B 6766
  CALL &GROUP.SET_GID
  FLAG 1 :changed
update.1:
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host group3.test", <<'EOF', "group creation without explicit GID";
FLAG 0 :changed
;; res_group group3
CALL &USERDB.OPEN
OK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
SET %A 1
SET %B "group3"
CALL &GROUP.FIND
OK? @found.1
  COPY %B %A
  CALL &GROUP.NEXT_GID
  COPY %R %B
  CALL &GROUP.CREATE
  FLAG 1 :changed
  JUMP @update.1
found.1:
update.1:
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host group4.test", <<'EOF', "group with all attrs";
FLAG 0 :changed
;; res_group group4
CALL &USERDB.OPEN
OK? @start.1
  PRINT "Failed to open the user databases\n"
  HALT
start.1:
SET %A 1
SET %B "group4"
CALL &GROUP.FIND
OK? @found.1
  COPY %B %A
  SET %B 6778
  CALL &GROUP.CREATE
  FLAG 1 :changed
  JUMP @update.1
found.1:
  SET %B 6778
  CALL &GROUP.SET_GID
  FLAG 1 :changed
update.1:
SET %B "x"
CALL &GROUP.SET_PASSWD
SET %B "$$crypt"
CALL &GROUP.SET_PWHASH
  FLAG 1 :changed
;; members
SET %A "user1"
CALL &GROUP.HAS_MEMBER?
OK? @has-member.user1.1
  CALL &GROUP.ADD_MEMBER
  FLAG 1 :changed
has-member.user1.1:
SET %A "user2"
CALL &GROUP.HAS_MEMBER?
OK? @has-member.user2.1
  CALL &GROUP.ADD_MEMBER
  FLAG 1 :changed
has-member.user2.1:
SET %A "user3"
CALL &GROUP.HAS_MEMBER?
NOTOK? @no-member.user3.1
  CALL &GROUP.RM_MEMBER
  FLAG 1 :changed
no-member.user3.1:
;; admins
SET %A "adm1"
CALL &GROUP.HAS_ADMIN?
OK? @has-admin.adm1.1
  CALL &GROUP.ADD_ADMIN
  FLAG 1 :changed
has-admin.adm1.1:
SET %A "root"
CALL &GROUP.HAS_ADMIN?
NOTOK? @no-admin.root.1
  CALL &GROUP.RM_ADMIN
  FLAG 1 :changed
no-admin.root.1:
!FLAGGED? :changed @next.1
  CALL &USERDB.SAVE
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

done_testing;
