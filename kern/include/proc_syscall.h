
#ifndef _PROC_SYSCALL_H_
#define _PROC_SYSCALL_H_


int sys_getpid(pid_t *retval);

void sys_exit(int exitcode);


#endif /* _PROC_SYSCALL_H */