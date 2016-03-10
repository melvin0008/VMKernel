
#ifndef _PROC_SYSCALL_H_
#define _PROC_SYSCALL_H_


int
sys_getpid(pid_t *retval);

void
sys_exit(int exitcode);

int
sys_waitpid(pid_t pid, int *status, int options, pid_t *retval);


#endif /* _PROC_SYSCALL_H */