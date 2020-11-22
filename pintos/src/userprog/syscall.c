#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include <list.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "devices/shutdown.h"
#include "pagedir.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/off_t.h"
#include "lib/kernel/console.h"
#include "lib/user/syscall.h"
#define NumberCall 21
#define stdin 1

typedef void (*handler)(struct intr_frame *f);

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

void syscall_init(void)
{
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
  handlers[SYS_HALT] = SysHalt;  //强制停止
  handlers[SYS_EXIT] = SysExit;  //退出
  handlers[SYS_EXEC] = SysExec;  //生成子进程
  handlers[SYS_WAIT] = SysWait;  //等待子进程
  handlers[SYS_CREATE] = SysCreate; //新建文件
  handlers[SYS_REMOVE] = SysRemove; //删除文件
  handlers[SYS_OPEN] = SysOpen;     //打开文件
  handlers[SYS_FILESIZE] = SysFileSize; //文件大小
  handlers[SYS_READ] = SysRead;       //读文件
  handlers[SYS_WRITE] = SysWrite;     //写文件
  handlers[SYS_SEEK] = SysSeek;       //仅可读文件
  handlers[SYS_TELL] = SysTell;     //分辨文件
  handlers[SYS_CLOSE] = SysClose;//关闭文件
}

static void
syscall_handler(struct intr_frame *f)
{
  //printf ("system call!\n");
  //shutdown_power_off();
  validateAddr((const void *)f->esp);
  uint32_t call_number = getSyscallNumber(f);
  handlers[call_number](f);
  //thread_exit();
}

/*用来判断系统调用给我的地址有没有问题*/
bool validateAddr(void *vaddr)
{
  if (vaddr != NULL && vaddr < PHYS_BASE && is_user_vaddr(vaddr) && pagedir_get_page(thread_current()->pagedir, vaddr))
  {
    return true;
  }
  else
  {
    exit(-1);
  }
}

/*用来获取一个系统调用的编号*/
uint32_t getSyscallNumber(struct intr_frame *f)
{
  return *((uint32_t *)f->esp);
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
  f->eax = (uint32_t)ret_value;
  thread_exit();
}

/*写文件系统调用*/
/*int fd, char *buffer, unsigned size*/
void SysWrite(struct intr_frame *f)
{
  int *esp = f->esp;
  int fd = (int)getArguments(f, 1);
  const char *buffer = (const char *)getArguments(f, 2);
  unsigned size = (size_t)getArguments(f, 3);
  //printf("file descriptor:%d, second arguments:%x, buffer size:%d, fourth arguments:%d, fifth arguments:%d, sixth argument:%d\n", fd, buffer,size,*(esp+4), *(esp+5), *(esp+6));
  if (fd == 1)
  {

    while (size > 100)
    {
      putbuf(buffer, 100);
      buffer += 100;
      size -= 100;
      barrier();
    }
    putbuf(buffer, size);
  }
  else
  {
    if (fd <= 0)
    {
      thread_current()->ret = -1;
      thread_exit();
    }
    else if (fd > sizeof(thread_current()->fdtable) / sizeof(struct file *))
    {
      thread_current()->ret = -1;
      thread_exit();
    }
    else if (thread_current()->fdtable[fd] == NULL)
    {
      thread_current()->ret = -1;
      thread_exit();
    }
    else if ((get_user((uint8_t *)buffer) == -1) || (buffer == NULL))
    {
      thread_current()->ret = -1;
      thread_exit();
    }

    struct file *filetowrite = thread_current()->fdtable[fd];
    lock_acquire(&handlesem);
    off_t writebytes = file_write(filetowrite, (void *)buffer, (off_t)size);
    lock_release(&handlesem);
    f->eax = (int)writebytes;
  }
  barrier();
}

/*创建一个新文件*/
/*char* file_name, unsigned initial_size*/
void SysCreate(struct intr_frame *f)
{
  bool ok = false;
  const char *file_name = (char *)getArguments(f, 1);
  validateAddr(file_name);
  if (*file_name=='\0'){
    exit(-1);
  }
  else{
    char *file; //文件指针

    file = palloc_get_page(0); //文件指针分配一页内存
    if (file == NULL)          //如果没分配到
    {
      f->eax = -1; //返回-1
    }
    strlcpy(file, *(char **)file_name, PGSIZE); //文件页复制栈帧中的内容
    unsigned initial_size = getArguments(f, 2);
    if ((strlen(file) <= 14) && (strlen(file) > 0)) //如果文件名长度<=14或者>=0
    {
      lock_acquire(&handlesem);
      ok = filesys_create(file, initial_size); //创建文件
      lock_release(&handlesem);
    }
    palloc_free_page(file); //释放页
  }
  f->eax = (int)ok;
  barrier();
}

/*打开一个文件,获取它的文件描述符*/
/*char* filename*/
void SysOpen(struct intr_frame *f)
{
  const char *file_name = (char *)getArguments(f, 1);
  validateAddr(file_name);
  char *file; //文件名
  file = palloc_get_page(0); //为这个指针分配一个页
  if (file == NULL)          //分配失败
  {
    f->eax = -1;
  }
  else{
    strlcpy(file, *(char **)file_name, PGSIZE);
    lock_acquire(&handlesem);
    struct file *new_file_position = filesys_open(file_name);
    lock_release(&handlesem);
    palloc_free_page(file);
    barrier();

    int fd;
    if (new_file_position == NULL)
    {
      f->eax = -1;
    }
    else
    {
      fd = thread_current()->nextfd; //fd是当前进程下一个打开的文件
      if (fd >= 64)                  //如果文件数>64
      {
        fd = -1; //返回值-1
        lock_acquire(&handlesem);
        file_close(new_file_position); //关闭打开文件
        lock_release(&handlesem);
      }
      else
      {
        thread_current()->fdtable[fd] = new_file_position; //当前进程打开的文件又多了一个
        thread_current()->nextfd++;                 //指向下一个文件槽
      }
    }
    f->eax = fd;
    barrier();
  }
}

/*据文件描述符关闭一个文件*/
/*int fd*/
void SysClose(struct intr_frame *f)
{
  int fd = (int)getArguments(f, 1);
  if (fd < 0|| fd >64)
  {
    exit(-1);
  }
  else
  {
    struct list_elem *file_node = thread_current()->fdtable[fd];
    //printf("target fd:%d\n", fd);
    if (file_node != NULL)
    {
      thread_current()->fdtable[fd] = NULL;
      lock_acquire(&handlesem);
      file_close(file_node);
      lock_release(&handlesem);
      filenum--;
    }
    else
    {
      exit(-1);
    }
    barrier();
  }
  f->eax = 0;
}

/*生成子进程*/
void SysExec(struct intr_frame *f){
  char *tempesp = (char *)f->esp;       //新栈指针
  tempesp += 4;                         //新栈指针上移4位
  if (*(uint32_t *)tempesp > PHYS_BASE) //如果此时栈指针不在用户栈中
  {
    exit(-1);
  }
  if ((*tempesp == NULL) || (get_user(*(uint8_t **)tempesp) == -1)) //同上
  {
    exit(-1);
  }
  char *cmd_line; //命令行

  cmd_line = palloc_get_page(0); //从页中获得命令行

  if (cmd_line == NULL) //如果命令行是空的
  {
    f->eax = -1; //存入返回值为空
    // return -1;
  }
  else{
    strlcpy(cmd_line, *(char **)tempesp, PGSIZE); //拷贝命令行

    // lock_acquire(&handlesem);
    // struct file* openedfile = filesys_open(cmd_line);
    // lock_release(&handlesem);

    int answer = (int)process_execute(cmd_line); //运行进程

    f->eax = answer; //返回值是运行后的返回值
    // file_close(openedfile);
    palloc_free_page(cmd_line); //销毁命令行
  }
}

/*等待子进程*/
void SysWait(struct intr_frame *f) {
  char *tempesp = (char *)f->esp; //新栈指针

  tempesp += 4;                  //栈指针上移4位
  pid_t pid = *(pid_t *)tempesp; //将pid从栈帧中取出
  // printf("pid : %d\n", pid);
  int answer = process_wait((tid_t)pid);
  f->eax = answer; //返回值是线程等待函数的返回值
}

/*删除文件*/
void SysRemove(struct intr_frame *f){
  char *tempesp = (char *)f->esp;
  tempesp += 4;
  if (*(uint32_t *)tempesp > PHYS_BASE) //非用户空间
    thread_exit();
  char *file; //文件名

  file = palloc_get_page(0); //为这个指针分配一个页
  if (file == NULL)          //分配失败
  {
    f->eax = -1; //返回-1
    // return -1;
  }
  else{
    strlcpy(file, *(char **)tempesp, PGSIZE);
    lock_acquire(&handlesem);
    bool answer = filesys_remove(file); //删除文件
    lock_release(&handlesem);

    f->eax = answer;        //返回值
    palloc_free_page(file); //释放页
    barrier();
  }
}

/*文件大小*/
void SysFileSize(struct intr_frame *f){
  char *tempesp = (char *)f->esp; //新栈指针
  tempesp += 4;                   //新栈指针上移4位
  int fd = *(int *)tempesp;
  struct file *filetoread = thread_current()->fdtable[fd]; //找到这个文件
  int filesize;
  if (filetoread == NULL) //没这玩意，返回-1
    filesize = -1;
  else
  {
    lock_acquire(&handlesem);
    filesize = (int)(file_length(filetoread)); //读取文件大小
    lock_release(&handlesem);
  }

  f->eax = filesize; //返回大小值
  barrier();
}

/*读文件*/
void SysRead(struct intr_frame *f){
  char *tempesp = (char *)f->esp;

  tempesp += 4;
  int fd = *(int *)tempesp;
  tempesp += 4;
  if ((fd < 0) || (fd > 64)) //文件号不合法
  {
    thread_current()->ret = -1;
    thread_exit();
  }
  if (*(uint32_t *)tempesp > PHYS_BASE) //非用户空间
  {
    thread_current()->ret = -1;
    thread_exit();
  }

  if ((get_user(*(uint8_t **)tempesp) == -1) || (tempesp == NULL)) //同上
  {
    thread_current()->ret = -1;
    thread_exit();
  }
  char *buffer;

  buffer = *(char **)tempesp;
  tempesp += 4;
  unsigned size = *(unsigned *)tempesp;
  unsigned i;
  uint8_t fdzero;
  if (fd == 0)
  {
    for (i = 0; i < size; i++)
    {
      fdzero = input_getc();
      *buffer = (char)fdzero;
      buffer++;
    }
    f->eax = size;
  }
  else if (fd > 1)
  {
    if (size == 0)
    {
      f->eax = 0;
    }
    else
    {
      struct file *filetoread = thread_current()->fdtable[fd];
      if (filetoread == NULL)
        f->eax = -1;
      else if (get_user(*(uint8_t **)filetoread) == -1)
      {
        f->eax = -1;
      }
      else
      {
        barrier();
        lock_acquire(&handlesem);
        off_t readbytes = file_read(filetoread, (void *)buffer, (off_t)size);
        lock_release(&handlesem);
        f->eax = (uint32_t)readbytes;
      }
    }
  }
  barrier();
}

/*仅可读文件*/
void SysSeek(struct intr_frame *f){
  char *tempesp = (char *)f->esp;

  tempesp += 4;
  int fd = *(int *)tempesp;
  tempesp += 4;
  unsigned position = *(unsigned *)tempesp;
  struct file *filetoseek = thread_current()->fdtable[fd];
  if (filetoseek == NULL)
  {
    ;
  }
  else
  {
    lock_acquire(&handlesem);
    file_seek(filetoseek, (off_t)position);
    lock_release(&handlesem);
  }
}

/*分辨文件*/
void SysTell(struct intr_frame *f){
  char *tempesp = (char *)f->esp;

  tempesp += 4;
  int fd = *(int *)tempesp;
  struct file *filetotell = thread_current()->fdtable[fd];
  if (filetotell == NULL){
    ;
  }
  else
  {
    lock_acquire(&handlesem);
    unsigned answertell = (unsigned)file_tell(filetotell);
    lock_release(&handlesem);
    f->eax = answertell;
  }
}

/*从一个进程中退出*/
void exit(int status)
{
  printf("%s: exit(%d)\n", thread_current()->name, status);
  thread_current()->ret = status;
  thread_exit();
}
