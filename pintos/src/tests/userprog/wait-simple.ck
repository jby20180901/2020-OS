# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected ([<<'EOF']);
(waited-simple) begin
(child-simple) run
child-simple: exit(81)
(waited-simple) waited(exec()) = 81
(waited-simple) end
waited-simple: exit(0)
EOF
pass;
