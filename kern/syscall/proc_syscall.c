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
#include <kern/fcntl.h>
#include <../arch/mips/include/trapframe.h>
#include <addrspace.h>
#include <syscall.h>
#include <vfs.h>

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
* Reference : http://jhshi.me/2012/03/11/os161-fork-system-call/index.html
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
    struct trapframe *child_tf = (struct trapframe*) kmalloc(sizeof(struct trapframe));
    if(parent_tf==NULL || child_tf==NULL){
        if(child_tf!=NULL){
            kfree(child_tf);
            child_tf=NULL;
        }
        return ENOMEM;
    }

    struct proc *child_proc = proc_create_runprogram((const char *) "child_proc");
    child_proc->ppid=curproc->pid;

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
    
    err = thread_fork("child_proc", child_proc,(void *) child_forkentry, child_tf,(long unsigned int) NULL);
    if (err)
    {
        return err;
    }
    // voidchild_thread;
    *retval = child_proc->pid;
    return 0;
}

/* Reference :
http://jhshi.me/2012/03/11/os161-execv-system-call/index.html
*/

int
sys_execv(const char *program_name, char **args){
    (void ) args;
    int err;
    char kernel_program_name[NAME_MAX];
    size_t actual;
    struct addrspace *as;
    struct vnode *vn;
    vaddr_t entrypoint, stackptr;
    int result; 

    //Check for validations and copy name in kernel space
    err = copyinstr((const_userptr_t) program_name, kernel_program_name, NAME_MAX, &actual);
    if (err != 0){ 
        return err; 
    } 

    //Opens the file
    result = vfs_open(kernel_program_name, O_RDONLY, 0, &vn);
    if (result) {
        return result;
    }

    /* We should be a new process. */
    KASSERT(proc_getas() == NULL);

    /* Create a new address space. */
    as = as_create();
    if (as == NULL) {
        vfs_close(vn);
        return ENOMEM;
    }

    /* Switch to it and activate it. */
    proc_setas(as);
    as_activate();

    /* Load the executable. */
    result = load_elf(vn, &entrypoint);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        vfs_close(vn);
        return result;
    }

    /* Done with the file now. */
    vfs_close(vn);

    /* Define the user stack in the address space */
    result = as_define_stack(as, &stackptr);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        return result;
    }

    enter_new_process(0 /*argc*/, NULL /*userspace addr of argv*/,
              NULL /*userspace addr of environment*/,
              stackptr, entrypoint);

    /* enter_new_process does not return. */
    panic("enter_new_process returned\n");
    return EINVAL;
}


