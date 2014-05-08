#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::policy::common;

gencode_ok "use host sysctl1.test", <<'EOF', "sysctl resource";
;; res_sysctl net.ipv6.icmp.ratelimit
SET %A "/proc/sys/net/ipv6/icmp/ratelimit"
CALL &FS.GET
COPY %S2 %T1
SET %T2 "1005"
CMP? @diff.1
  JUMP @done.1
diff.1:
  COPY %T2 %B
  CALL &FS.PUT
done.1:
next.1:
EOF

gencode_ok "use host sysctl2.test", <<'EOF', "persistent sysctl resource";
;; res_sysctl net.ipv6.icmp.ratelimit
SET %A "/proc/sys/net/ipv6/icmp/ratelimit"
CALL &FS.GET
COPY %S2 %T1
SET %T2 "0"
CMP? @diff.1
  JUMP @done.1
diff.1:
  COPY %T2 %B
  CALL &FS.PUT
done.1:
SET %A "/files/etc/sysctl.conf/%s"
SET %B "0"
SET %C "net.ipv6.icmp.ratelimit"
CALL &AUGEAS.SET
next.1:
EOF


done_testing;
