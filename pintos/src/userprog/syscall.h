#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

void syscall_init (void);
extern struct lock handlesem;//读写锁
extern int filenum;//最多打开文件数量
#endif /* userprog/syscall.h */
