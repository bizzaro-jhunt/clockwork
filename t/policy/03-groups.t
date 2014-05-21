#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host group1.test", <<'EOF', "group removal";
;; res_group group1
SET %A 1
SET %B "group1"
CALL &GROUP.FIND
OK? @next.1
  CALL &GROUP.REMOVE
next.1:
EOF

gencode_ok "use host group2.test", <<'EOF', "group creation with explicit GID";
;; res_group group2
SET %A 1
SET %B "group2"
CALL &GROUP.FIND
OK? @found.1
  COPY %B %A
  SET %B 6766
  CALL &GROUP.CREATE
  JUMP @update.1
found.1:
  SET %B 6766
  CALL &GROUP.SET_GID
update.1:
next.1:
EOF

gencode_ok "use host group3.test", <<'EOF', "group creation without explicit GID";
;; res_group group3
SET %A 1
SET %B "group3"
CALL &GROUP.FIND
OK? @found.1
  COPY %B %A
  CALL &GROUP.NEXT_GID
  COPY %R %B
  CALL &GROUP.CREATE
  JUMP @update.1
found.1:
update.1:
next.1:
EOF

gencode_ok "use host group4.test", <<'EOF', "group with all attrs";
;; res_group group4
SET %A 1
SET %B "group4"
CALL &GROUP.FIND
OK? @found.1
  COPY %B %A
  SET %B 6778
  CALL &GROUP.CREATE
  JUMP @update.1
found.1:
  SET %B 6778
  CALL &GROUP.SET_GID
update.1:
SET %B "x"
CALL &GROUP.SET_PASSWD
SET %B "$$crypt"
CALL &GROUP.SET_PWHASH
;; members
SET %A "user1"
CALL &GROUP.HAS_MEMBER?
OK? @member.user1.else.1
  CALL &GROUP.ADD_MEMBER
member.user1.else.1:
SET %A "user2"
CALL &GROUP.HAS_MEMBER?
OK? @member.user2.else.1
  CALL &GROUP.ADD_MEMBER
member.user2.else.1:
SET %A "user3"
CALL &GROUP.HAS_MEMBER?
NOTOK? @member.user3.else.1
  CALL &GROUP.RM_MEMBER
member.user3.else.1:
;; admins
SET %A "adm1"
CALL &GROUP.HAS_ADMIN?
OK? @admin.adm1.else.1
  CALL &GROUP.ADD_ADMIN
admin.adm1.else.1:
SET %A "root"
CALL &GROUP.HAS_ADMIN?
NOTOK? @admin.root.else.1
  CALL &GROUP.RM_ADMIN
admin.root.else.1:
next.1:
EOF

done_testing;
