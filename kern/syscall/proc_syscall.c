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


    if(is_proc_null(pid) ||  (!is_pid_in_range(pid))){
        return ESRCH;
    }
    // TODO check if correct

    if(options != 0 && options != WNOHANG && options != WUNTRACED){
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
        lock_release(child->exit_lk);
        return 0;
    }

    if(!child->is_exited){
        cv_wait(child->exit_cv, child->exit_lk);
    }
    if(status!=NULL){
        error_val = copyout(&child->exit_code, (userptr_t)status, sizeof(int));
        if(error_val){
            return error_val;
        }
    }
    *retval = pid;
    lock_release(child->exit_lk);
    proc_destroy(child);

    return 0;
}

/*
Reference : http://jhshi.me/2012/03/11/os161-fork-system-call/index.html
*/

void 
child_forkentry(void * tf_ptr, unsigned long data2)
{
    (void ) data2;
    struct trapframe tf;
    // int err;
    // err=copyout(tf_ptr,(userptr_t) &tf,sizeof(struct trapframe));
    // if(err){
    //     return;
    // }
    tf=*(struct trapframe*)tf_ptr;
    as_activate();
    // tf=tf_ptr;
    // tf=tf_ptr;
    
    tf.tf_v0 = 0;
    tf.tf_a3 = 0;
    tf.tf_epc += 4;
    kfree(tf_ptr);
    mips_usermode(&tf);
}



int 
sys_fork(struct trapframe *parent_tf, pid_t *retval){
    // (void) parent_tf;
    // (void) retval;

    struct proc *child_proc = init_proc((const char *) "child_proc");
    child_proc->ppid=curproc->pid;

    struct trapframe *child_tf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
    if(parent_tf==NULL || child_tf==NULL){
        // kfree(child_tf);
        child_tf=NULL;
        return ENOMEM;
    }

    *child_tf = *parent_tf;
    int err;
    // err=copyout((const void *)parent_tf, (userptr_t) child_tf,sizeof(struct trapframe));
    // if(err){
    //     return err;
    // }
    err = as_copy(curproc->p_addrspace,&child_proc->p_addrspace);
    if(err){
        return err;
    }
    // as_activate(child_as);
    // pid_t child_thread_pid;
    
    err = thread_fork("child_proc", child_proc, (void *) child_forkentry, child_tf,(long unsigned int) NULL);
    if (err)
    {
        return err;
    }
    // voidchild_thread;
    *retval = child_proc->pid;
    return 0;
}



