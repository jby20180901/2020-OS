#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#define USERPROG 1

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. 运行态*/
    THREAD_READY,       /* Not running but ready to run. 就绪态*/
    THREAD_BLOCKED,     /* Waiting for an event to trigger. 等待态*/
    THREAD_DYING        /* About to be destroyed. 终止态*/
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. tid=-1，错误*/

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. 最低优先级0*/
#define PRI_DEFAULT 31                  /* Default priority. 默认优先级31*/
#define PRI_MAX 63                      /* Highest priority. 最高优先级63*/

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |栈空间
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. 进程状态*/
    char name[16];                      /* Name (for debugging purposes). 进程名称*/
    uint8_t *stack;                     /* Saved stack pointer. 栈指针*/
    int priority;                       /* Priority. 优先级*/
    struct list_elem allelem;           /* List element for all threads list. 进程表*/

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    char *process_stack;                /* 用户栈 */
    int ret;                            /* 返回值 */
    struct file *fdtable[64];           /* 已打开的文件列表 */
    int nextfd;                         /* 下一个文件指针 */
    struct list childrenlist;           /* 子进程列表 */
    struct list_elem child_elem;        /* 作为一个子进程，存在父进程的elem */
    struct semaphore startLoadSem;      /* 子进程加载信号量 */
    struct semaphore returnLoadSem;      /* 子进程是否加载成功 */
    struct semaphore waitsem;           /* 父进程等待子进程的信号量 */
    struct semaphore dieSem;            /* 进程死亡信号量 */
    struct semaphore recycleSem;        /* 父进程等待回收子进程的资源 */
    struct semaphore inforDeathSem;            /*  */
    bool loadsuccess;                   /* 是否load进程成功的标志 */
    struct file *file;                  /* 当前打开的文件 */
    bool wait;                          /* 是否在等待子进程 */
#endif

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. 魔数*/
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

#endif /* threads/thread.h */
