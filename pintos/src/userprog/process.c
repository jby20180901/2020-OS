#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "syscall.h"

static thread_func start_process NO_RETURN;
static bool load(const char *cmdline, void (**eip)(void), void **esp);
int a = 0;//
struct thread *find_child(tid_t giventid)//寻找子进程
{
  struct list *templist = &thread_current()->childrenlist;//当前进程的子进程序列
  if (list_empty(templist))//如果是空的
    return NULL;//返回NULL
  struct list_elem *e;
  for (e = list_begin(templist); e != list_end(templist);
       e = list_next(e))//遍历
  {
    struct thread *t = list_entry(e, struct thread, child_elem);
    if (t->tid == giventid)
      return t;//返回这个子进程
  }
  return NULL;
}

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t process_execute(const char *file_name)//父进程创建子进程
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME.
	   Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page(0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy(fn_copy, file_name, PGSIZE);

  char *token, *save_ptr;
  token = strtok_r(file_name, " ", &save_ptr);//将文件名截断出来

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create(token, PRI_DEFAULT, start_process, fn_copy);//生成子进程
  struct thread *child = find_child(tid);//找到这个子进程
  sema_down(&child->loadsem);//等待子进程加载完毕
  bool child_load_success = child->loadsuccess;//此时子进程加载完毕
  sema_up(&child->loadsuccesssem);//把子进程从子进程加载成功信号量释放出来
  if (!child_load_success)//如果子进程没加载成功
  {
    //For exec-missing case
    sema_down(&child->exitsem);//当前进程阻塞在子进程退出信号量，等待子进程退出
    return -1;//返回-1
  }
  // printf("WWW %d \n",filenum);
  if (tid == TID_ERROR)//如果tid 是-1
    palloc_free_page(fn_copy);//释放这个页面
  return tid;
}

/* A thread function that loads a user process and makes it start
   running. */
static void
start_process(void *f_name)//进程开始运行
{
  char *file_name = f_name;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset(&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load(file_name, &if_.eip, &if_.esp);

  thread_current()->loadsuccess = success;//当前进程的加载状态success
  sema_up(&thread_current()->loadsem);//作为子进程，我已经加载成功了，释放等待我加载的父进程
  sema_down(&thread_current()->loadsuccesssem);//当前进程阻塞在loadsuccesssem上，等待父进程确认自己的加载状态

  // palloc_free_page (file_name);
  palloc_free_page(f_name);//清空页
  if (!success)
  {
    thread_current()->ret = -1;//失败了，返回值为-1
    thread_exit();
  }

  /* Start the user process by simulating a return from an
	   interrupt, implemented by intr_exit (in
	   threads/intr-stubs.S).  Because intr_exit takes all of its
	   arguments on the stack in the form of a `struct intr_frame',
	   we just point the stack pointer (%esp) to our stack frame
	   and jump to it. */

  thread_current()->nextfd = 2;

  asm volatile("movl %0, %%esp; jmp intr_exit"
               :
               : "g"(&if_)
               : "memory");
  NOT_REACHED();
}

/* This is 2016 spring cs330 skeleton code */

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
     child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.
   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int process_wait(tid_t child_tid)//父进程等待子进程
{
  // printf("WWW %d \n",filenum);
  // timer_sleep((int64_t)100);
  struct thread *child = find_child(child_tid);//找到子进程
  if (child == NULL)//如果没有子进程
  {
    return -1;//返回-1
  }
  sema_down(&child->waitsem);//父进程阻塞在这个子进程的等待序列，等待子进程回收

  // printf("child name : %s\n", child->name);

  if (child->wait)//如果子进程在等
  {
    return -1;//返回-1
  }
  child->wait = true;//子进程等状态置为true

  int exitstatus = child->ret;//退出状态是子进程的退出状态
  if (child->file != NULL)//子进程打开文件不是空
  {
    lock_acquire(&handlesem);//获得读写锁
    file_allow_write(child->file);//允许写子进程当前操作的文件
    lock_release(&handlesem);//释放读写锁
  }
  sema_up(&child->diesem);//把子进程从死亡信号量上释放，继续它的回收操作
  sema_down(&child->jinsem);//阻塞在子进程的jinsem上，等待子进程将自身从父进程列表中删除

  child->wait = false;//子进程等待撞态变为false
  barrier();
  return exitstatus;//返回子进程的退出状态
}

/* Free the current process's resources. */
void process_exit(void)//进程销毁，回收资源
{
  struct thread *curr = thread_current();
  uint32_t *pd;
  /* Destroy the current process's page directory and switch back

	   to the kernel-only page directory. */
  pd = curr->pagedir;//当前页表
  if (pd != NULL)
  {
    /* Correct ordering here is crucial.  We must set
		   cur->pagedir to NULL before switching page directories,
		   so that a timer interrupt can't switch back to the
		   process page directory.  We must activate the base page
		   directory before destroying the process's page
		   directory, or our active page directory will be one
		   that's been freed (and cleared). */
    curr->pagedir = NULL;//当前页表置为空
    pagedir_activate(NULL);//活跃页表置为空
    pagedir_destroy(pd);//销毁页表
    sema_up(&curr->exitsem);//等待当前进程退出的进程释放
  }

  if (strcmp(curr->name, "main") != 0)//不是main函数
  {
    printf("%s: exit(%d)\n", curr->name, curr->ret);//输出退出状态

    sema_up(&curr->waitsem);//当前进程的等待信号释放
    sema_down(&curr->diesem);//当前进程阻塞死亡序列，等待它的父进程来回收它
    list_remove(&curr->child_elem);//把当前进程从父进程的子进程列表中移除
    sema_up(&curr->jinsem);//释放等待自己操作的父进程

    // if(!list_empty(&curr->exitsem.waiters))
  }
  else//是main函数
  {
    sema_up(&curr->waitsem); //当前进程的等待信号释放
  }

  int j;
  for (j = 2; j < 64; j++)//遍历已打开的文件列表
  {
    struct file *ftoclose = curr->fdtable[j];
    if (ftoclose != NULL)//如果此时文件不是空
    {
      lock_acquire(&handlesem);
      file_close(ftoclose);//关闭这个文件
      lock_release(&handlesem);
      filenum--;//文件数--
    }
  }
  if (curr->file != NULL)//如果当前使用的文件不是空
  {
    lock_acquire(&handlesem);//获得锁
    file_close(curr->file);//关闭文件
    lock_release(&handlesem);//释放锁
    filenum--;//文件数--
    curr->file = NULL;//当前使用的文件为空
  }

  barrier();
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void process_activate(void)//程序活跃
{
  struct thread *t = thread_current();

  /* Activate thread's page tables. */
  pagedir_activate(t->pagedir);//页表活跃

  /* Set thread's kernel stack for use in processing
	   interrupts. */
  tss_update();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32 /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32 /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32 /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16 /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
  unsigned char e_ident[16];
  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off e_phoff;
  Elf32_Off e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL 0           /* Ignore. */
#define PT_LOAD 1           /* Loadable segment. */
#define PT_DYNAMIC 2        /* Dynamic linking info. */
#define PT_INTERP 3         /* Name of dynamic loader. */
#define PT_NOTE 4           /* Auxiliary info. */
#define PT_SHLIB 5          /* Reserved. */
#define PT_PHDR 6           /* Program header table. */
#define PT_STACK 0x6474e551 /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1 /* Executable. */
#define PF_W 2 /* Writable. */
#define PF_R 4 /* Readable. */

static bool setup_stack(void **esp, char *file_name);
static bool validate_segment(const struct Elf32_Phdr *, struct file *);
static bool load_segment(struct file *file, off_t ofs, uint8_t *upage,
                         uint32_t read_bytes, uint32_t zero_bytes,
                         bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */

bool load(const char *file_name, void (**eip)(void), void **esp)//加载
{
  struct thread *t = thread_current();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create();//新建页表
  if (t->pagedir == NULL)//如果页表是空
    goto done;//返回
  process_activate();//进程激活

  char *fn_copy;

  fn_copy = palloc_get_page(0);//获得一个页
  if (fn_copy == NULL)//分配失败
    return TID_ERROR;
  strlcpy(fn_copy, file_name, PGSIZE);
  /* Open executable file. */
  char *token, *save_ptr;
  token = strtok_r(file_name, " ", &save_ptr);//获取文件名

  lock_acquire(&handlesem);
  file = filesys_open(token);//打开文件
  lock_release(&handlesem);
  filenum++;//打开的文件数++

  if (file == NULL)//文件是空
  {

    printf("load: %s: open failed\n", file_name);
    goto done;
  }
  file_deny_write(file);//禁止文件写
  t->file = file;//当前进程的正在使用文件为这个文件

  /* Read and verify executable header. */
  if (file_read(file, &ehdr, sizeof ehdr) != sizeof ehdr || memcmp(ehdr.e_ident, "\177ELF\1\1\1", 7) || ehdr.e_type != 2 || ehdr.e_machine != 3 || ehdr.e_version != 1 || ehdr.e_phentsize != sizeof(struct Elf32_Phdr) || ehdr.e_phnum > 1024)
  {
    printf("load: %s: error loading executable\n", file_name);//读取文件失败
    goto done;
  }

  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
  {
    struct Elf32_Phdr phdr;

    if (file_ofs < 0 || file_ofs > file_length(file))
      goto done;
    file_seek(file, file_ofs);

    if (file_read(file, &phdr, sizeof phdr) != sizeof phdr)
      goto done;
    file_ofs += sizeof phdr;
    switch (phdr.p_type)
    {
    case PT_NULL:
    case PT_NOTE:
    case PT_PHDR:
    case PT_STACK:
    default:
      /* Ignore this segment. */
      break;
    case PT_DYNAMIC:
    case PT_INTERP:
    case PT_SHLIB:
      goto done;
    case PT_LOAD:
      if (validate_segment(&phdr, file))
      {
        bool writable = (phdr.p_flags & PF_W) != 0;
        uint32_t file_page = phdr.p_offset & ~PGMASK;
        uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
        uint32_t page_offset = phdr.p_vaddr & PGMASK;
        uint32_t read_bytes, zero_bytes;
        if (mem_page == 0)
          mem_page = 0x10000000;
        if (phdr.p_filesz > 0)
        {
          /* Normal segment.
					   Read initial part from disk and zero the rest. */
          read_bytes = page_offset + phdr.p_filesz;
          zero_bytes = (ROUND_UP(page_offset + phdr.p_memsz, PGSIZE) - read_bytes);
        }
        else
        {
          /* Entirely zero.
					   Don't read anything from disk. */
          read_bytes = 0;
          zero_bytes = ROUND_UP(page_offset + phdr.p_memsz, PGSIZE);
        }
        if (!load_segment(file, file_page, (void *)mem_page,
                          read_bytes, zero_bytes, writable))
          goto done;
      }
      else
        goto done;
      break;
    }
  }

  /* Set up stack. */
  if (!setup_stack(esp, fn_copy))
    // if (!setup_stack (esp, file_name))
    goto done;

  /* Start address. */
  *eip = (void (*)(void))ehdr.e_entry;

  success = true;

done:
  /* We arrive here whether the load is successful or not. */
  palloc_free_page(fn_copy);
  // file_close(file);
  return success;
}

/* load() helpers. */

static bool install_page(void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment(const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off)file_length(file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
	   user address space range. */
  if (!is_user_vaddr((void *)phdr->p_vaddr))
    return false;
  if (!is_user_vaddr((void *)(phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
	   address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
	   Not only is it a bad idea to map page 0, but if we allowed
	   it then user code that passed a null pointer to system calls
	   could quite likely panic the kernel by way of null pointer
	   assertions in memcpy(), etc. */

  // if (phdr->p_vaddr < PGSIZE)
  //   return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:
        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.
        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.
   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment(struct file *file, off_t ofs, uint8_t *upage,
             uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT(pg_ofs(upage) == 0);
  ASSERT(ofs % PGSIZE == 0);

  file_seek(file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
  {
    /* Do calculate how to fill this page.
		   We will read PAGE_READ_BYTES bytes from FILE
		   and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Get a page of memory. */
    uint8_t *kpage = palloc_get_page(PAL_USER);
    if (kpage == NULL)
      return false;

    /* Load this page. */
    if (file_read(file, kpage, page_read_bytes) != (int)page_read_bytes)
    {
      palloc_free_page(kpage);
      return false;
    }
    memset(kpage + page_read_bytes, 0, page_zero_bytes);

    /* Add the page to the process's address space. */
    if (!install_page(upage, kpage, writable))
    {
      palloc_free_page(kpage);
      return false;
    }

    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
  }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack(void **esp, char *file_name)
{
  uint8_t *kpage;
  bool success = false;
  char *fn_copy;

  /* Make a copy of FILE_NAME.
	   Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page(0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy(fn_copy, file_name, PGSIZE);

  kpage = palloc_get_page(PAL_USER | PAL_ZERO);
  if (kpage != NULL)
  {
    success = install_page(((uint8_t *)PHYS_BASE) - PGSIZE, kpage, true);
    if (success)
    {
      *esp = PHYS_BASE;//esp指向物理基地址
      uint8_t *jinesp = (uint8_t *)*esp;//jinesp = esp
      char *token, *token2, *save_ptr, *save_ptr2;//划分文件
      int argc = 0, arglength = 1;//参数个数0，参数表长度1
      int jinjin = 0;//规定的置0位
      for (token = strtok_r(file_name, " ", &save_ptr); token != NULL;
           token = strtok_r(NULL, " ", &save_ptr))//遍历参数表
      {
        argc++;//参数++
        arglength += (int)(strlen(token)) + 1;//参数表字节长度++
      }
      jinesp -= arglength;//jinesp移动到要放入真实参数的地方
      jinesp -= (argc + 1) * 4;//(参数总数+参数表头地址)*4字节
      jinesp -= 12;//向下移动3字节，安全保障
      *esp -= (arglength + 12 + (argc + 1) * 4);//真正的esp向下移动

      thread_current()->process_stack = (char *)*esp;//用户栈空间指向esp

      *(int *)jinesp = jinjin;
      jinesp += 4;//返回值，此时是0，入栈
      *(int *)jinesp = argc;
      jinesp += 4;//参数个数入栈
      *(uint32_t *)jinesp = (uint32_t)(jinesp + 4);
      jinesp += 4;//栈指针入栈
      int temp = 4 * (argc + 1) + 1;//交换？？？
      for (token2 = strtok_r(fn_copy, " ", &save_ptr2); token2 != NULL;
           token2 = strtok_r(NULL, " ", &save_ptr2))
      {//重新遍历参数表
        *(uint32_t *)jinesp = (uint32_t)(jinesp + temp);
        jinesp += temp;
        strlcpy((char *)jinesp, token2, (size_t)(strlen(token2) + 1));
        jinesp -= temp;
        jinesp += 4;
        temp -= 4;
        temp += strlen(token2) + 1;
      }
      *(int *)jinesp = jinjin;
      uint8_t woowoo = (uint8_t)0;
      jinesp += 4;
      *jinesp = woowoo;
      // hex_dump (*esp, *esp, (size_t)(arglength+12+(argc+1)*4),true);
    }
    else
    {
      palloc_free_page(kpage);
    }
  }
  palloc_free_page(fn_copy);

  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page(void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current();

  /* Verify that there's not already a page at that virtual
	   address, then map our page there. */
  return (pagedir_get_page(t->pagedir, upage) == NULL && pagedir_set_page(t->pagedir, upage, kpage, writable));
}
