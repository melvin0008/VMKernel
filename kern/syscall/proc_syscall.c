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
#include <syscall.h>
#include <vfs.h>
#include <lib.h>
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


    if((!is_pid_in_range(pid)) || is_proc_null(pid)){
        return ESRCH;
    }
    if(status == KERN_PTR || status == INVAL_PTR){
        return EFAULT;
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
    if(child_proc==NULL){
        kfree(child_tf);
        return ENOMEM;
    }
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

static void free_exec_mem(char** corrected_args , char* kernel_program_name, int * kargv_length,int total){
    int i;
    for(i=0;i<total;i++){
        kfree(corrected_args[i]);
    }
    kfree(corrected_args);
    kfree(kernel_program_name);
    kfree(kargv_length);
}

int
sys_execv(const char *program_name, char **args){
    (void ) args;
    int err;
    size_t actual;
    struct addrspace *as;
    struct vnode *vn;
    vaddr_t entrypoint, stackptr;
    vaddr_t temp_stackptr;
    int result;

    if(program_name==NULL || program_name ==INVAL_PTR || program_name == KERN_PTR ){
        return EFAULT;
    }

    char* kernel_program_name = (char*) kmalloc(NAME_MAX);
    err = copyinstr((const_userptr_t) program_name, kernel_program_name, NAME_MAX, &actual);
    if (err){ 
        kfree(kernel_program_name);
        return err; 
    } 
    int i;
    if(strlen(kernel_program_name)==0){
        kfree(kernel_program_name);
        return EINVAL;
    }
    // int prog_length = strlen(kernel_program_name) + 1;
   
    //Check for validations and copy name in kernel space
    struct addrspace *parent_addrspace = curproc->p_addrspace;
    if(args==NULL  || (args== INVAL_PTR) || (args  == KERN_PTR)){
        kfree(kernel_program_name);
        return EFAULT;
    }

    int total = 0;
    for(i = 0; args[i] != NULL; i++){
        if( args[i]== INVAL_PTR ||  args[i]== KERN_PTR){
            kfree(kernel_program_name);
            return EFAULT;
        }
        total++;
    }
    if(args==NULL || total==0 ){
        kfree(kernel_program_name);
        return EFAULT;
    }

    char** corrected_args = (char **) kmalloc (sizeof(char*)*total);
    err = copyin((const_userptr_t) args, corrected_args,sizeof(char **));
    if(err){ 
        kfree(kernel_program_name);
        kfree(corrected_args);
        return err;
    } 
    int* kargv_length = (int*) kmalloc(sizeof(int)*total);
    int k;
    int total_length = 0;
    for(k = 0; k < total; k++){

        int len = strlen(args[k]);
        int padding = 0;
        if((len % 4) != 0){
            padding = 4 - (len%4);    
        }
        else{
            padding=4;
        }

        *(kargv_length+k) = len + padding;
        total_length += len + padding;
        

        corrected_args[k] = (char *) kmalloc(sizeof(char)*(len+padding));
        err = copyin((const_userptr_t) args[k],corrected_args[k],len);
        if(err){
            free_exec_mem(corrected_args,kernel_program_name,kargv_length,total);
            return err;
        }

        for(int j = len; j< len+padding; j++){
            *(corrected_args[k] + j) = '\0';
        }
    }
    total_length += ((total + 1) * 4);

    //Opens the file
    result = vfs_open(kernel_program_name, O_RDONLY, 0, &vn);
    if (result) {
        vfs_close(vn);
        free_exec_mem(corrected_args,kernel_program_name,kargv_length,total);
        return result;
    }
    
    /* Create a new address space. */
    as = as_create();
    if (as == NULL) {
        vfs_close(vn);
        free_exec_mem(corrected_args,kernel_program_name,kargv_length,total);
        return ENOMEM;
    }

    /* Switch to it and activate it. */
    proc_setas(as);
    as_activate();

    /* Load the executable. */
    result = load_elf(vn, &entrypoint);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        free_exec_mem(corrected_args,kernel_program_name,kargv_length,total);
        as_destroy(parent_addrspace);
        vfs_close(vn);
        return result;
    }

    /* Done with the file now. */
    vfs_close(vn);

    /* Define the user stack in the address space */
    result = as_define_stack(as, &stackptr);
    if (result) {
        /* p_addrspace will go away when curproc is destroyed */
        free_exec_mem(corrected_args,kernel_program_name,kargv_length,total);
        return result;
    }
    int pointers_length = 4 * (total + 1);
    stackptr -= total_length;
    vaddr_t offset = stackptr + pointers_length;
    vaddr_t temp_offset1 = offset;
    for(i=0;i <total;i++){
        err = copyout(*(corrected_args+i),(userptr_t)temp_offset1, *(kargv_length+i));
        if (err) {
            free_exec_mem(corrected_args,kernel_program_name,kargv_length,total);
            return err;
        }    
        temp_offset1+=*(kargv_length+i);
    }
    
    vaddr_t temp_offset = offset;
    temp_stackptr = stackptr;
    for(i = 0; i < total; i++){
        copyout( &temp_offset, (void *) temp_stackptr,4);
        temp_stackptr+=4;
        temp_offset+=*(kargv_length+i);
    }
    int temp = 0;
    copyout( &temp,(void *) temp_stackptr, 4);

    free_exec_mem(corrected_args,kernel_program_name,kargv_length,total);
    as_destroy(parent_addrspace);
    // kprintf()
    enter_new_process(total, (userptr_t) stackptr /*userspace addr of argv*/,
              NULL /*userspace addr of environment*/,
              stackptr, entrypoint);

    /* enter_new_process does not return. */
    panic("enter_new_process returned\n");
    return EINVAL;
}

/// Reference : http://jhshi.me/2012/04/27/os161-sbrk-system-call/index.html
int
sys_sbrk(int32_t increment, vaddr_t *retval){
    increment += (increment % 4);

    if(increment <= -(1024*4096*256)){
        return EINVAL;
    }
    // (void) retval;
    struct addrspace *as = proc_getas();

    if(as->heap_end + increment >= as->stack_end){
        retval = (void *) -1;
        return ENOMEM;
    }
    if(as->heap_end + increment < as->heap_start){
        retval = (void *) -1;
        return EINVAL;
    }
    // kprintf("increment is %d \n", increment);
    if(increment < 0){
        // Remove all the addresses in between
        vaddr_t hi_addr = as->heap_end;
        vaddr_t low_addr = as->heap_end + increment;
        vaddr_t i;
        for(i = low_addr; i <= hi_addr; i+= PAGE_SIZE){
            remove_pte_for(as, i);
        }

    }
    *retval = as->heap_end;
    as->heap_end = as->heap_end + increment;
    return 0;    
}