/* Wait for a subprocess to finish, twice.
   The first call must waited in the usual way and return the exit code.
   The second waited call must return -1 immediately. */

#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"

void
test_main (void) 
{
  pid_t child = exec ("child-simple");
  msg ("waited(exec()) = %d", waited (child));
  msg ("waited(exec()) = %d", waited (child));
}
