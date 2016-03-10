/*
* Process Syscall Implementations
* Header file in proc_syscall.h
*/

#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/filehandle.h>
#include <file_syscall.h>
#include <limits.h>
#include <current.h>
#include <proc.h>
#include <proc_syscall.h>
#include <kern/wait.h>

int sys_getpid(pid_t *retval){
    *retval = curproc->pid;
    return 0;
}

void sys_exit(int exitcode){
    int current_pid = curproc->pid;
    //TODO: Check for valid range of exitcode
    struct proc *current_proc = get_proc(current_pid);
    lock_acquire(current_proc->exit_lk);
    current_proc->is_exited = true;
    current_proc->exit_code = _MKWAIT_EXIT(exitcode);
    lock_release(current_proc->exit_lk);
    thread_exit(); 
}
