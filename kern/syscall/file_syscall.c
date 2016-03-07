/*
File Syscall Implementations
Header file in file_syscall.h
*/

#include <types.h>
#include <vnode.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <kern/filehandle.h>
#include <copyinout.h>
#include <uio.h>
#include <vfs.h>
#include <file_syscall.h>
#include <stat.h>
#include <limits.h>
#include <current.h>

/*
Reference:
http://jhshi.me/2012/03/28/os161-arguments-passing-in-system-call/index.html
http://jhshi.me/2012/03/14/os161-file-system-calls/index.html
*/
int sys_open(userptr_t filename,int flag,int *fd){
    
    char kbuf[NAME_MAX]; 
    int err,result;
    size_t actual;
    struct vnode *vn;
    off_t offset = 0;
    struct fhandle *fh;
    int i;
    if ((err = copyinstr((const_userptr_t) filename, kbuf, NAME_MAX, &actual)) != 0){ 
        return err; 
    } 
    result = vfs_open(kbuf,flag, 0664 , &vn);
    if (result) {
        return result;
    }
    struct stat stat_offset;
    if( flag & O_APPEND ){
        result=VOP_STAT(vn,&stat_offset);
        if (result) {
           return result;
        }
        offset=stat_offset.st_size;
    }
    for( i = 0; i<OPEN_MAX; i++){
        if(curthread->t_ftable[i]==NULL){
            break;
        }
    }
    if( i == OPEN_MAX ){
        return EMFILE;
    }
    fh=fhandle_create((const char*)filename,vn,offset,flag);
    if(fh==NULL){
        return ENOMEM;
    }
    curthread->t_ftable[i]=fh;
    *fd=i;
    return 0;
}
