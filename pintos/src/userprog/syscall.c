#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "lib/user/syscall.h"
#include "threads/vaddr.h"
#include "lib/kernel/console.h"
#include "filesys/off_t.h"
#include "devices/shutdown.h"
#include "pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"

#define NumberCall 21
#define stdin 1

typedef void (*handler)(struct intr_frame *f);

handler handlers[NumberCall];
/*获得系统调用的编号*/
int getSyscallNumber(struct intr_frame *f);
/*获得pos处的系统调用参数*/
void *getArguments(struct intr_frame *f, int pos);
/*判断一个地址有没有问题*/
bool validateAddr(void *vaddr);

static void syscall_handler(struct intr_frame *);

/* 用来应对halt系统调用 */
void SysHalt(struct intr_frame *);

/* 用来应对Exit系统调用 */
void SysExit(struct intr_frame *);

/* 用来应对Write系统调用*/
void SysWrite(struct intr_frame *);

/*用来应对create系统调用*/
void SysCreate(struct intr_frame *f);

/*用来应对open系统调用*/
void SysOpen(struct intr_frame *f);

/*用来应对close系统调用*/
void SysClose(struct intr_frame *f);
/*d Reads a byte at user virtual address UADDR.
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

struct lock handlesem;
bool already = false;
bool jungwoo = false;
int filenum = 0;
void
syscall_init (void)
{
	intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  handlers[SYS_HALT] = SysHalt;
  handlers[SYS_WRITE] = SysWrite;
  handlers[SYS_EXIT] = SysExit;
  handlers[SYS_CREATE] = SysCreate;
  handlers[SYS_OPEN] = SysOpen;
  handlers[SYS_CLOSE] = SysClose;
}

static void
syscall_handler(struct intr_frame *f)
{
  validateAddr((const void *)f->esp);
  int call_number = getSyscallNumber(f);
  handlers[call_number](f);
}

/*用来判断系统调用给我的地址有没有问题*/
bool validateAddr(void *vaddr)
{
  if (vaddr != NULL && vaddr < PHYS_BASE && is_user_vaddr(vaddr) && pagedir_get_page(thread_current()->pagedir, vaddr) )
  {
    return true;
  }
  else
  {
    exit(-1);
  }
}

/*用来获取一个系统调用的编号*/
int getSyscallNumber(struct intr_frame *f)
{
  return *((int *)f->esp);
}

/*用来获取一个系统调用的参数*/
void *getArguments(struct intr_frame *f, int pos)
{
  validateAddr((int *)f->esp + pos);
  int *stack_top = f->esp;
  stack_top += pos;
  return (void *)(*stack_top);
}

/*关机系统调用,没有参数*/
void SysHalt(struct intr_frame *f)
{
  shutdown_power_off();
}

/*退出进程系统调用*/
/*int*/
void SysExit(struct intr_frame *f)
{
  int ret_value = getArguments(f, 1);
  printf("%s: exit(%d)\n", thread_current()->name, ret_value);
  //struct thread *cur = thread_current();
  //cur->ret = ret_value;
  thread_current()->ret = ret_value;
  f->eax = 0;
  thread_exit();
}

/*写文件系统调用*/
/*int fd, char *buffer, unsigned size*/
void SysWrite(struct intr_frame *f)
{
  int *esp = f->esp;
  int fd = (int)getArguments(f, 1);
  const char *buffer = (const char *)*(esp + 8);
  unsigned size = (size_t)getArguments(f, 3);
  //printf("file descriptor:%d, second arguments:%x, buffer size:%d, fourth arguments:%d, fifth arguments:%d, sixth argument:%d\n", fd, buffer,size,*(esp+4), *(esp+5), *(esp+6));
  putbuf(buffer, size);
  f->eax = 0;
}

/*创建一个新文件*/
/*char* file_name, unsigned initial_size*/
void SysCreate(struct intr_frame *f)
{
  const char *file_name = (char *)getArguments(f, 4);
  validateAddr(file_name);
  unsigned initial_size = getArguments(f, 2);
  //printf("file_name:%s\n", file_name);
  bool ok = filesys_create(file_name, initial_size);
  f->eax = (int)ok;
}

/*打开一个文件,获取它的文件描述符*/
/*char* filename*/
void SysOpen(struct intr_frame *f)
{
  const char *file_name = (char *)getArguments(f, 1);
  validateAddr(file_name);
  lock_acquire(&filesys_lock);
  struct file *new_file_position = filesys_open(file_name);
  lock_release(&filesys_lock);
  if (new_file_position == NULL)
  {
    f->eax = -1;
  }
  else
  {
    struct thread *cur = thread_current();
    struct opened_file *new_file = malloc(sizeof(struct opened_file));
    //printf("current max fd:%d\n", cur->max_fd);
    new_file->fd = (cur->max_fd) + 1;
    new_file->position = new_file_position;
    cur->max_fd++;
    list_push_back(&cur->list_opened_file, &new_file->node);
    //printf("file descriptor:%d\n", new_file->fd);
    f->eax = new_file->fd;
  }
}

/*据文件描述符关闭一个文件*/
/*int fd*/
void SysClose(struct intr_frame *f)
{
  int fd = (int)getArguments(f, 1);
  if (fd <= 1)
  {
    exit(-1);
  }
  else
  {
    struct list_elem *file_node = get_file_by_fd(fd);
    //printf("target fd:%d\n", fd);
    if (file_node != NULL)
    {
      //printf("not null!\n");
      struct opened_file *to_be_closed = list_entry(file_node, struct opened_file, node);
      list_remove(file_node);
      lock_acquire(&filesys_lock);
      file_close(to_be_closed->position);
      lock_release(&filesys_lock);
    }
    else
    {
      //printf("not found!\n");
      exit(-1);
    }
  }
  f->eax = 0;
}
/*从一个进程中退出*/
void exit(int status)
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_current()->ret = status;
  thread_exit();
}

//   case SYS_WRITE:
//   {
//     char *tempesp = (char *)f->esp;

//     tempesp += 4;
//     int fd = *(int *)tempesp;
//     tempesp += 4;
//     if (*(uint32_t *)tempesp > PHYS_BASE)
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }

//     char *buffer = *(char **)tempesp;
//     tempesp += 4;
//     unsigned size = *(unsigned *)tempesp, temp;
//     if (fd == 1)
//     {

//       while (size > 100)
//       {
//         putbuf(buffer, 100);
//         buffer += 100;
//         size -= 100;
//         barrier();
//       }
//       putbuf(buffer, size);
//     }
//     else
//     {
//       if (fd <= 0)
//       {
//         thread_current()->returnstatus = -1;
//         thread_exit();
//       }
//       else if (fd > sizeof(thread_current()->fdtable) / sizeof(struct file *))
//       {
//         thread_current()->returnstatus = -1;
//         thread_exit();
//       }
//       else if (thread_current()->fdtable[fd] == NULL)
//       {
//         thread_current()->returnstatus = -1;
//         thread_exit();
//       }
//       else if ((get_user((uint8_t *)buffer) == -1) || (buffer == NULL))
//       {
//         thread_current()->returnstatus = -1;
//         thread_exit();
//       }

//       struct file *filetowrite = thread_current()->fdtable[fd];
//       lock_acquire(&handlesem);
//       off_t writebytes = file_write(filetowrite, (void *)buffer, (off_t)size);
//       lock_release(&handlesem);
//       f->eax = (int)writebytes;
//     }
//     barrier();
//     break;
//   }

// static void
// syscall_handler(struct intr_frame *f)
// {
//   if ((f->esp > PHYS_BASE) || (get_user((uint8_t *)f->esp) == -1))//不是用户空间
//   {
//     thread_current()->returnstatus = -1;//当前进程返回值为-1
//     thread_exit();//进程结束
//   }
//   uint32_t num = *(uint32_t *)f->esp;//系统调用号

//   switch (num)
//   {
//   case SYS_HALT://强制停止
//   {
//     shutdown_power_off();//断电
//     break;
//   }
//   case SYS_EXIT://退出
//   {
//     char *tempesp = (char *)f->esp;//新栈指针
//     tempesp += 4;//新栈指针上移4位
//     if (*(uint32_t *)tempesp < PHYS_BASE)//如果此时栈指针还处于用户空间
//       thread_current()->returnstatus = get_user((uint8_t *)tempesp);//从栈帧处取出返回值
//     else
//       thread_current()->returnstatus = -1;//否则，返回值为-1
//     thread_exit();//进程结束
//     break;
//   }
//   case SYS_EXEC://执行
//   {
//     char *tempesp = (char *)f->esp;//新栈指针
//     tempesp += 4;//新栈指针上移4位
//     if (*(uint32_t *)tempesp > PHYS_BASE)//如果此时栈指针不在用户栈中
//     {
//       thread_current()->returnstatus = -1;//返回值-1
//       thread_exit();//进程退出
//     }
//     if ((*tempesp == NULL) || (get_user(*(uint8_t **)tempesp) == -1))//同上
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }
//     char *cmd_line;//命令行

//     cmd_line = palloc_get_page(0);//从页中获得命令行

//     if (cmd_line == NULL)//如果命令行是空的
//     {
//       f->eax = -1;//存入返回值为空
//       break;
//       // return -1;
//     }
//     strlcpy(cmd_line, *(char **)tempesp, PGSIZE);//拷贝命令行

//     // lock_acquire(&handlesem);
//     // struct file* openedfile = filesys_open(cmd_line);
//     // lock_release(&handlesem);

//     int answer = (int)process_execute(cmd_line);//运行进程

//     f->eax = answer;//返回值是运行后的返回值
//     // file_close(openedfile);
//     palloc_free_page(cmd_line);//销毁命令行

//     break;
//   }

//     /*Waits for a child process pid and retrieves the child's exit status.

//    If pid is still alive, waits until it terminates. Then, returns the status
//    that pid passed to exit. If pid did not call exit(), but was terminated by the
//    kernel (e.g. killed due to an exception), wait(pid) must return -1. It is perfectly legal for a parent process to wait for child processes that have already terminated by the time the parent calls wait, but the kernel must still allow the parent to retrieve its child's exit status, or learn that the child was terminated by the kernel.

//    wait must fail and return -1 immediately if any of the following conditions is true:

//    pid does not refer to a direct child of the calling process. pid is a direct child of the calling process if and only if the calling process received pid as a return value from a successful call to exec.
//    Note that children are not inherited: if A spawns child B and B spawns child process C, then A cannot wait for C, even if B is dead. A call to wait(C) by process A must fail. Similarly, orphaned processes are not assigned to a new parent if their parent process exits before they do.
//    The process that calls wait has already called wait on pid. That is, a process may wait for any given child at most once.
//    Processes may spawn any number of children, wait for them in any order, and may even exit without having waited for some or all of their children. Your design should consider all the ways in which waits can occur. All of a process's resources, including its struct thread, must be freed whether its parent ever waits for it or not, and regardless of whether the child exits before or after its parent.

//    You must ensure that Pintos does not terminate until the initial process exits. The supplied Pintos code tries to do this by calling process_wait()(in userprog/process.c) from main() (in threads/init.c). We suggest that you implement process_wait() according to the comment at the top of the function and then implement the wait system call in terms of process_wait().

//    Implementing this system call requires considerably more work than any of the rest.*/

//   case SYS_WAIT://等待的系统调用
//   {
//     char *tempesp = (char *)f->esp;//新栈指针

//     tempesp += 4;//栈指针上移4位
//     pid_t pid = *(pid_t *)tempesp;//将pid从栈帧中取出
//     // printf("pid : %d\n", pid);
//     int answer = process_wait((tid_t)pid);
//     f->eax = answer;//返回值是线程等待函数的返回值
//     break;
//   }
//   case SYS_CREATE://新建
//   {
//     char *tempesp = (char *)f->esp;//新栈指针
//     tempesp += 4;//新栈指针上移4位
//     if (*(uint32_t *)tempesp > PHYS_BASE)//非用户空间
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }

//     if ((*tempesp == NULL) || (get_user(*(uint8_t **)tempesp) == -1))//同上
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }
//     bool answer = false;//返回值默认false
//     if (*tempesp == '\0')//如果栈指针指向空字符串
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }
//     else
//     {
//       char *file;//文件指针

//       file = palloc_get_page(0);//文件指针分配一页内存
//       if (file == NULL)//如果没分配到
//       {
//         f->eax = -1;//返回-1
//         break;
//         // return -1;
//       }
//       strlcpy(file, *(char **)tempesp, PGSIZE);//文件页复制栈帧中的内容
//       tempesp += 4;//新栈指针上移4个字节
//       unsigned initial_size = *(unsigned *)tempesp;//初始化栈指针
//       if ((strlen(file) <= 14) && (strlen(file) > 0))//如果文件名长度<=14或者>=0
//       {
//         lock_acquire(&handlesem);
//         answer = filesys_create(file, initial_size);//创建文件
//         lock_release(&handlesem);
//       }
//       palloc_free_page(file);//释放页
//     }
//     f->eax = answer;//返回值
//     barrier();
//     break;
//   }
//   case SYS_REMOVE://删除
//   {
//     char *tempesp = (char *)f->esp;
//     tempesp += 4;
//     if (*(uint32_t *)tempesp > PHYS_BASE)//非用户空间
//       thread_exit();
//     char *file;//文件名

//     file = palloc_get_page(0);//为这个指针分配一个页
//     if (file == NULL)//分配失败
//     {
//       f->eax = -1;//返回-1
//       break;
//       // return -1;
//     }
//     strlcpy(file, *(char **)tempesp, PGSIZE);
//     lock_acquire(&handlesem);
//     bool answer = filesys_remove(file);//删除文件
//     lock_release(&handlesem);

//     f->eax = answer;//返回值
//     palloc_free_page(file);//释放页
//     barrier();
//     break;
//   }
//   case SYS_OPEN://打开文件
//   {
//     char *tempesp = (char *)f->esp;
//     tempesp += 4;
//     if (*(uint32_t *)tempesp > PHYS_BASE)
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//       f->eax = -1;
//     }

//     if ((get_user(*(uint8_t **)tempesp) == -1) || (*tempesp == NULL))
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//       f->eax = -1;
//     }
//     char *file;//文件名

//     file = palloc_get_page(0);//为这个指针分配一个页
//     if (file == NULL)//分配失败
//     {
//       f->eax = -1;
//       break;
//     }
//     strlcpy(file, *(char **)tempesp, PGSIZE);

//     lock_acquire(&handlesem);
//     struct file *openedfile = filesys_open(file);//找到要打开的文件指针
//     lock_release(&handlesem);
//     palloc_free_page(file);
//     barrier();

//     int fd;
//     if (openedfile == NULL)//没这玩意，返回-1
//     {
//       fd = -1;
//       // palloc_free_page(file);
//       // f->eax=fd;
//     }
//     else
//     {

//       fd = thread_current()->nextfd;//fd是当前进程下一个打开的文件
//       if (fd >= 64)//如果文件数>64
//       {
//         fd = -1;//返回值-1
//         lock_acquire(&handlesem);
//         file_close(openedfile);//关闭打开文件
//         lock_release(&handlesem);
//       }
//       else
//       {
//         thread_current()->fdtable[fd] = openedfile;//当前进程打开的文件又多了一个
//         thread_current()->nextfd++;//指向下一个文件槽
//       }
//     }
//     f->eax = fd;
//     barrier();
//     break;
//   }
//   case SYS_FILESIZE://文件大小
//   {
//     char *tempesp = (char *)f->esp;//新栈指针
//     tempesp += 4;//新栈指针上移4位
//     int fd = *(int *)tempesp;
//     struct file *filetoread = thread_current()->fdtable[fd];//找到这个文件
//     int filesize;
//     if (filetoread == NULL)//没这玩意，返回-1
//       filesize = -1;
//     else
//     {
//       lock_acquire(&handlesem);
//       filesize = (int)(file_length(filetoread));//读取文件大小
//       lock_release(&handlesem);
//     }

//     f->eax = filesize;//返回大小值
//     barrier();
//     break;
//   }

//   /*Reads size bytes from the file open as fd into buffer.
// 	   Returns the number of bytes actually read (0 at end of file), or -1
// 	   if the file could not be read (due to a condition other than end of file).
// 	   Fd 0 reads from the keyboard using input_getc().*/
//   case SYS_READ://读
//   {

//     char *tempesp = (char *)f->esp;

//     tempesp += 4;
//     int fd = *(int *)tempesp;
//     tempesp += 4;
//     if ((fd < 0) || (fd > 64))//文件号不合法
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }
//     if (*(uint32_t *)tempesp > PHYS_BASE)//非用户空间
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }

//     if ((get_user(*(uint8_t **)tempesp) == -1) || (tempesp == NULL))//同上
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }
//     char *buffer;

//     buffer = *(char **)tempesp;
//     tempesp += 4;
//     unsigned size = *(unsigned *)tempesp;
//     unsigned i;
//     uint8_t fdzero;
//     if (fd == 0)
//     {
//       for (i = 0; i < size; i++)
//       {
//         fdzero = input_getc();
//         *buffer = (char)fdzero;
//         buffer++;
//       }
//       f->eax = size;
//     }
//     else if (fd > 1)
//     {
//       if (size == 0)
//       {
//         f->eax = 0;
//       }
//       else
//       {
//         struct file *filetoread = thread_current()->fdtable[fd];
//         if (filetoread == NULL)
//           f->eax = -1;
//         else if (get_user(*(uint8_t **)filetoread) == -1)
//         {
//           f->eax = -1;
//         }
//         else
//         {
//           barrier();
//           lock_acquire(&handlesem);
//           off_t readbytes = file_read(filetoread, (void *)buffer, (off_t)size);
//           lock_release(&handlesem);
//           f->eax = (uint32_t)readbytes;
//         }
//       }
//     }
//     barrier();

//     break;
//   }
//   case SYS_WRITE:
//   {
//     char *tempesp = (char *)f->esp;

//     tempesp += 4;
//     int fd = *(int *)tempesp;
//     tempesp += 4;
//     if (*(uint32_t *)tempesp > PHYS_BASE)
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }

//     char *buffer = *(char **)tempesp;
//     tempesp += 4;
//     unsigned size = *(unsigned *)tempesp, temp;
//     if (fd == 1)
//     {

//       while (size > 100)
//       {
//         putbuf(buffer, 100);
//         buffer += 100;
//         size -= 100;
//         barrier();
//       }
//       putbuf(buffer, size);
//     }
//     else
//     {
//       if (fd <= 0)
//       {
//         thread_current()->returnstatus = -1;
//         thread_exit();
//       }
//       else if (fd > sizeof(thread_current()->fdtable) / sizeof(struct file *))
//       {
//         thread_current()->returnstatus = -1;
//         thread_exit();
//       }
//       else if (thread_current()->fdtable[fd] == NULL)
//       {
//         thread_current()->returnstatus = -1;
//         thread_exit();
//       }
//       else if ((get_user((uint8_t *)buffer) == -1) || (buffer == NULL))
//       {
//         thread_current()->returnstatus = -1;
//         thread_exit();
//       }

//       struct file *filetowrite = thread_current()->fdtable[fd];
//       lock_acquire(&handlesem);
//       off_t writebytes = file_write(filetowrite, (void *)buffer, (off_t)size);
//       lock_release(&handlesem);
//       f->eax = (int)writebytes;
//     }
//     barrier();
//     break;
//   }
//   case SYS_SEEK:
//   {
//     char *tempesp = (char *)f->esp;

//     tempesp += 4;
//     int fd = *(int *)tempesp;
//     tempesp += 4;
//     unsigned position = *(unsigned *)tempesp;
//     struct file *filetoseek = thread_current()->fdtable[fd];
//     if (filetoseek == NULL)
//       break;
//     else
//     {
//       lock_acquire(&handlesem);
//       file_seek(filetoseek, (off_t)position);
//       lock_release(&handlesem);
//       break;
//     }
//   }
//   case SYS_TELL:
//   {
//     char *tempesp = (char *)f->esp;

//     tempesp += 4;
//     int fd = *(int *)tempesp;
//     struct file *filetotell = thread_current()->fdtable[fd];
//     if (filetotell == NULL)
//       break;
//     else
//     {
//       lock_acquire(&handlesem);
//       unsigned answertell = (unsigned)file_tell(filetotell);
//       lock_release(&handlesem);
//       f->eax = answertell;

//       break;
//     }
//   }
//   case SYS_CLOSE:
//   {
//     char *tempesp = (char *)f->esp;

//     tempesp += 4;
//     int fd = *(int *)tempesp;
//     if ((fd < 0) || (fd > 64))
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }

//     struct file *filetoclose = thread_current()->fdtable[fd];
//     if (filetoclose != NULL)
//     {
//       thread_current()->fdtable[fd] = NULL;
//       lock_acquire(&handlesem);
//       file_close(filetoclose);
//       lock_release(&handlesem);
//       filenum--;
//     }
//     else
//     {
//       thread_current()->returnstatus = -1;
//       thread_exit();
//     }
//     barrier();
//     break;
//   }
//   case SYS_MMAP:
//   {
//     break;
//   }
//   case SYS_MUNMAP:
//   {
//     break;
//   }
//   case SYS_CHDIR:
//   {
//     break;
//   }
//   case SYS_MKDIR:
//   {
//     break;
//   }
//   case SYS_READDIR:
//   {
//     break;
//   }
//   case SYS_ISDIR:
//   {
//     break;
//   }
//   case SYS_INUMBER:
//   {
//     break;
//   }
//   default:
//     break;
//   }
//   // thread_exit ();
// }
