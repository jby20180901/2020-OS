# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF', <<'EOF']);
(waited-bad-pid) begin
(waited-bad-pid) end
waited-bad-pid: exit(0)
EOF
(waited-bad-pid) begin
waited-bad-pid: exit(-1)
EOF
pass;
