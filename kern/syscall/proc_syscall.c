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
#include <copyinout.h>
#include <proc_syscall.h>
#include <proc.h>
#include <kern/wait.h>
#include <../arch/mips/include/trapframe.h>
#include <addrspace.h>


void child_forkentry(void * tf_ptr, unsigned long data);

int sys_getpid(pid_t *retval){
    *retval = curproc->pid;
    return 0;
}

/*
Reference : http://jhshi.me/2012/03/12/os161-exit-and-waitpid-system-call/index.html
*/

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
    // TODO Is the status pointer properly aligned (by 4) ?
    // TODO Is the status pointer a valid pointer anyway (NULL, point to kernel, ...)?
    // TODO Is options valid? (More flags than WNOHANG | WUNTRACED )

    if(is_proc_null(pid)){
        return ESRCH;
    }
    // TODO check if correct
    if(!is_pid_in_range(pid) || (options != 0 && options != 1 && options != 2)){
        return EINVAL;
    }

    struct proc *child = get_proc(pid);
    int error_val;

    if(curproc->pid != child->ppid){
        return ECHILD;
    }

    lock_acquire(child->exit_lk);

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

/*
Reference : http://jhshi.me/2012/03/11/os161-fork-system-call/index.html
*/

// int child_forkentry(){

// }
void child_forkentry(void * tf_ptr, unsigned long data2)
{
    (void ) data2;
    struct trapframe *tf;
    int err;
    err=copyout(tf_ptr,(userptr_t) &tf,sizeof(struct trapframe));
    if(err){
        return;
    }
    // tf=tf_ptr;
    // tf=tf_ptr;
    kfree(tf_ptr);
    tf->tf_v0 = 0;
    tf->tf_a3 = 0;
    tf->tf_epc += 4;
    mips_usermode(tf);
}



int 
sys_fork(struct trapframe *parent_tf, pid_t *retval){
    (void) parent_tf;
    (void) retval;

    struct proc *child_proc = proc_create_runprogram((const char *) "child_proc");
    child_proc->ppid=curproc->pid;

    struct trapframe *child_tf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
    if(parent_tf==NULL || child_tf==NULL){
        kfree(child_tf);
        return ENOMEM;
    }
    // memmove(&parent_tf,child_tf,sizeof(struct trapframe));
    child_tf = parent_tf;
    int err;
    // err=copyout((const void *)parent_tf, (userptr_t) child_tf,sizeof(struct trapframe));
    // if(err){
    //     return err;
    // }
    struct addrspace *child_as;
    err=as_copy(curproc->p_addrspace,&child_as);
    if(err){
        return err;
    }

    err = thread_fork("child_proc", child_proc, child_forkentry, child_tf,(long unsigned int) NULL);
    if (err)
    {
        return err;
    }
    *retval = child_proc->pid;

    // copyout()
    // as_copy()
    // thread_fork()
    return 0;
}



