# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(waited-twice) begin
(child-simple) run
child-simple: exit(81)
(waited-twice) waited(exec()) = 81
(waited-twice) waited(exec()) = -1
(waited-twice) end
waited-twice: exit(0)
EOF
pass;
