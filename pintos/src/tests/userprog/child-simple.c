/* Child process run by exec-multiple, exec-one, waited-simple, and
   waited-twice tests.
   Just prints a single message and terminates. */

#include <stdio.h>
#include "tests/lib.h"

int
main (void) 
{
  test_name = "child-simple";

  msg ("run");
  return 81;
}
