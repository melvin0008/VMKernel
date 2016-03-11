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
#include <copyinout.h>
#include <proc_syscall.h>
#include <kern/wait.h>

int sys_getpid(pid_t *retval){
    *retval = curproc->pid;
    return 0;
}

void sys_exit(int exitcode){
    //TODO: Check for valid range of exitcode
    lock_acquire(curproc->exit_lk);
    
    curproc->is_exited = true;
    curproc->exit_code = _MKWAIT_EXIT(exitcode);
    
    cv_signal(curproc->exit_cv, curproc->exit_lk);
    lock_release(curproc->exit_lk);
    
    thread_exit();
    // Check if parent exists
    if((curproc->ppid) && get_proc(curproc->ppid)){
        proc_destroy(curproc);
    }
}

int
sys_waitpid(pid_t pid, int *status, int options, pid_t *retval){

    (void) status;
    (void) options;
    (void) retval;
    
    if(status % 4 || status == NULL){
        return EFAULT;
    }

    if(is_proc_null(pid)){
        return ESRCH;
    }
    // TODO check if correct
    if(!is_pid_in_range(pid) || (options != 0 && options != WNOHANG && options != WUNTRACED)){
        return EINVAL;
    }

    struct proc *child = get_proc(pid);
    int error_val;

    if(curproc->pid != child->ppid){
        return ECHILD;
    }

    lock_acquire(child->exit_lk);
    // Check for Non blocking call
    if(!child->is_exited && options == WNOHANG){
        *retval = 0;
        return 0;
    }

    while(!child->is_exited){
        cv_wait(child->exit_cv, child->exit_lk);
    }
    *retval = pid;

    error_val = copyout(&child->exit_code, (userptr_t)status, sizeof(int));
    
    if(error_val){
        return error_val;
    }

    lock_release(child->exit_lk);
    proc_destroy(child);

    return 0;
}