#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::policy::common;

gencode_ok "use host user1.test", <<'EOF', "user removal";
;; res_user t1user
SET %A 1
SET %B "t1user"
CALL &USER.FIND
OK? @next.1
  CALL &USER.REMOVE
next.1:
EOF

gencode_ok "use host user2.test", <<'EOF', "user creation with explicit UID/GID";
;; res_user t2user
SET %A 1
SET %B "t2user"
CALL &USER.FIND
OK? @create.1
  JUMP @check.ids.1
create.1:
SET %C 1818
SET %B 1231
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
next.1:
EOF

gencode_ok "use host user3.test", <<'EOF', "user creation without UID/GID";
;; res_user t3user
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
SET %B "x"
CALL &USER.SET_PASSWD
SET %B "$$crypto"
CALL &USER.SET_PWHASH
JUMP @exists.1
check.ids.1:
exists.1:
next.1:
EOF

gencode_ok "use host user4.test", <<'EOF', "user with all attrs";
;; res_user t4user
SET %A 1
SET %B "t4user"
CALL &USER.FIND
OK? @create.1
  JUMP @check.ids.1
create.1:
SET %C 1818
SET %B 1231
CALL &USER.CREATE
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
next.1:
EOF

done_testing;
