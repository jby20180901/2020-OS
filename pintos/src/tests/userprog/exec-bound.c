/* Exec a child with an exec string that spans a page boundary. */

#include <syscall.h>
#include "tests/userprog/boundary.h"
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  waited (exec (copy_string_across_boundary("child-args arg1 arg2")));
}
