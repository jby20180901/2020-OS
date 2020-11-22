# -*- perl -*-
use strict;
use warnings;
use tests::tests;
check_expected (IGNORE_EXIT_CODES => 1, [<<'EOF']);
(page-parallel) begin
(page-parallel) exec "child-linear"
(page-parallel) exec "child-linear"
(page-parallel) exec "child-linear"
(page-parallel) exec "child-linear"
(page-parallel) waited for child 0
(page-parallel) waited for child 1
(page-parallel) waited for child 2
(page-parallel) waited for child 3
(page-parallel) end
EOF
pass;
