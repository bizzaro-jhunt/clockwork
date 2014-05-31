#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

gencode_ok "use host service1.test", <<'EOF', "service resource";
RESET
;; res_service snmpd
SET %A "cwtool svc-enable snmpd"
CALL &EXEC.CHECK
SET %A "cwtool svc-init snmpd start"
CALL &EXEC.CHECK
FLAG 0 :res0
!FLAGGED? :res0 @next.1
SET %A "cwtool svc-init snmpd restart"
CALL &EXEC.CHECK
FLAG 0 :res0
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host service2.test", <<'EOF', "stopped service";
RESET
;; res_service microcode
SET %A "cwtool svc-init microcode stop"
CALL &EXEC.CHECK
FLAG 0 :res0
!FLAGGED? :res0 @next.1
SET %A "cwtool svc-init microcode stop"
CALL &EXEC.CHECK
FLAG 0 :res0
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF

gencode_ok "use host service3.test", <<'EOF', "service resource";
RESET
;; res_service neverwhere
SET %A "cwtool svc-disable neverwhere"
CALL &EXEC.CHECK
SET %A "cwtool svc-init neverwhere stop"
CALL &EXEC.CHECK
FLAG 0 :res0
!FLAGGED? :res0 @next.1
SET %A "cwtool svc-init neverwhere stop"
CALL &EXEC.CHECK
FLAG 0 :res0
next.1:
!FLAGGED? :changed @final.1
final.1:
EOF


done_testing;
