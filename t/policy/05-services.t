#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host service1.test", <<'EOF', "service resource";
FLAG 0 :changed
;; res_service snmpd
SET %A "cwtool svc-enable snmpd"
CALL &EXEC.CHECK
SET %A "cwtool svc-init start snmpd"
CALL &EXEC.CHECK
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host service2.test", <<'EOF', "stopped service";
FLAG 0 :changed
;; res_service microcode
SET %A "cwtool svc-init stop microcode"
CALL &EXEC.CHECK
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host service3.test", <<'EOF', "service resource";
FLAG 0 :changed
;; res_service neverwhere
SET %A "cwtool svc-disable neverwhere"
CALL &EXEC.CHECK
SET %A "cwtool svc-init stop neverwhere"
CALL &EXEC.CHECK
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF


done_testing;
