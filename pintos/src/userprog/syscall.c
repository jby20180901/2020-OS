#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/user/syscall.h"
#include "threads/vaddr.h"
#include "lib/kernel/console.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

/* Reads a byte at user virtual address UADDR.
   UADDR must be below PHYS_BASE.
   Returns the byte value if successful, -1 if a segfault
   occurred. */
static int
get_user(const uint8_t *uaddr)
{
  int result;
  asm("movl $1f, %0; movzbl %1, %0; 1:"
      : "=&a"(result)
      : "m"(*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
   UDST must be below PHYS_BASE.
   Returns true if successful, false if a segfault occurred. */
static bool
put_user(uint8_t *udst, uint8_t byte)
{
  int error_code;
  asm("movl $1f, %0; movb %b2, %1; 1:"
      : "=&a"(error_code), "=m"(*udst)
      : "q"(byte));
  return error_code != -1;
}

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  uint32_t num = *(uint32_t *)f->esp;
  switch(num)
  {
    case SYS_HALT:
    {
      power_off();
      break;
    }
    case SYS_EXIT:
    {

      break;
    }
    case SYS_EXEC:
    {

      break;
    }
    case SYS_WAIT:
    {

      break;
    }
    case SYS_CREATE:
    {

      break;
    }
    case SYS_REMOVE:
    {

      break;
    }
    case SYS_OPEN:
    {

      break;
    }
    case SYS_FILESIZE:
    {

      break;
    }
    case SYS_READ:
    {

      break;
    }
    case SYS_WRITE:
    {

      break;
    }
    case SYS_SEEK:
    {

      break;
    }
    case SYS_TELL:
    {

      break;
    }
    case SYS_CLOSE:
    {

      break;
    }
    case SYS_MMAP:
    {

      break;
    }
    case SYS_MUNMAP:
    {

      break;
    }
    case SYS_CHDIR:
    {

      break;
    }
    case SYS_MKDIR:
    {

      break;
    }
    case SYS_READDIR:
    {

      break;
    }
    case SYS_ISDIR:
    {

      break;
    }
    case SYS_INUMBER:
    {

      break;
    }
    default:
      break;
  }
  // thread_exit ();
}
