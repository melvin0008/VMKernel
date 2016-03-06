/*
File Syscall Implementations
Header file in file_syscall.h
*/

#include <types.h>
#include <vnode.h>
#include <kern/fcntl.h>
#include <copyinout.h>
#include <uio.h>
#include <vfs.h>
#include <file_syscall.h>


int sys_open(userptr_t filename,int flag,int *fd){

    (void) filename;
    (void) flag;
    (void) fd;

    return 0;
}
