# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(waited-killed) begin
(child-bad) begin
child-bad: exit(-1)
(waited-killed) waited(exec()) = -1
(waited-killed) end
waited-killed: exit(0)
EOF
pass;
