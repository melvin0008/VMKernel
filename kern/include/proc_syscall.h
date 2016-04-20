
#ifndef _PROC_SYSCALL_H_
#define _PROC_SYSCALL_H_
#include <../arch/mips/include/trapframe.h>

int sys_getpid(pid_t *retval);

void sys_exit(int exitcode);

int sys_waitpid(pid_t pid, int *status, int options, pid_t *retval);

int sys_fork(struct trapframe *tf, pid_t *retval);

int sys_execv(const char *program_name, char **args);

int sys_sbrk(int32_t increment, vaddr_t *retval);

#endif /* _PROC_SYSCALL_H */