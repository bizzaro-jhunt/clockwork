#!/usr/bin/perl
use strict;
use warnings;

use Test::More;
use t::common;

cwpol_ok "use host acl1.test; show acls", <<'EOF', "global ACLs";
  allow %systems "*" final
  allow %dev "show *"
  allow juser "service restart *"
  deny %probate "*"
EOF

cwpol_ok "use host acl2.test; show acls", <<'EOF', "conditional ACLs";
  allow %systems "*" final
  allow %example "show *"
  allow %hpux "show *" final
  deny %hpux "*"
EOF

cwpol_ok "use host acl3.test; show acls", <<'EOF', "multi-policy ACLs";
  allow %systems "*" final
  allow %dev "show *"
  allow juser "service restart *"
  deny %probate "*"
  allow %systems "*" final
  allow %example "show *"
  allow %hpux "show *" final
  deny %hpux "*"
EOF

cwpol_ok "use host acl4.test; show acls", <<'EOF', "inherited ACLs";
  allow %pre "show version"
  allow %base "query package *"
  allow %level1 "exec netstat" final
  allow %systems "*"
EOF

cwpol_ok "use host acl5.test; show acls", <<'EOF', "ALL keyword";
  allow %group "*"
EOF

cwpol_ok "use host acl6.test; show acls", <<'EOF', "unquoted strings";
  allow user "ping"
EOF

gencode_ok "use host acl1.test", <<'EOF', "pendulum code generated";
ACL "allow %systems \"*\" final"
ACL "allow %dev \"show *\""
ACL "allow juser \"service restart *\""
ACL "deny %probate \"*\""
EOF

done_testing;
