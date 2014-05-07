#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::policy::common;

gencode_ok "use host service1.test", <<'EOF', "service resource";
;; res_service snmpd
SET %A "cwtool service enable snmpd"
CALL &EXEC.CHECK
SET %A "cwtool service start snmpd"
CALL &EXEC.CHECK
next.1:
EOF

gencode_ok "use host service2.test", <<'EOF', "stopped service";
;; res_service microcode
SET %A "cwtool service stop microcode"
CALL &EXEC.CHECK
next.1:
EOF

gencode_ok "use host service3.test", <<'EOF', "service resource";
;; res_service neverwhere
SET %A "cwtool service disable neverwhere"
CALL &EXEC.CHECK
SET %A "cwtool service stop neverwhere"
CALL &EXEC.CHECK
next.1:
EOF


done_testing;
