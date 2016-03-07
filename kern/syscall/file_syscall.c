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
#include <proc.h>

/*
Reference:
http://jhshi.me/2012/03/28/os161-arguments-passing-in-system-call/index.html
http://jhshi.me/2012/03/14/os161-file-system-calls/index.html
*/

int
sys_open(userptr_t filename,int flag,int *fd)
{
    
    char kernel_buffer[NAME_MAX]; 
    int err,result;
    size_t actual;
    struct vnode *vn;
    off_t offset = 0;
    struct fhandle *fh;
    int i;

    err = copyinstr((const_userptr_t) filename, kernel_buffer, NAME_MAX, &actual);
    if (err != 0){ 
        return err; 
    } 
    result = vfs_open(kernel_buffer,flag, 0664 , &vn);
    if (result) {
        return result;
    }
    struct stat stat_offset;
    if( flag & O_APPEND ){
        result = VOP_STAT(vn,&stat_offset);
        if (result) {
           return result;
        }
        offset=stat_offset.st_size;
    }
    // Check for the first available slot
    for( i = 0; i<OPEN_MAX; i++){
        if(is_fh_null(i)){
            break;
        }
    }
    if(!is_valid_file_descriptor(i)){
        return EMFILE;
    }
    fh = fhandle_create((const char*)filename,vn,offset,flag);
    if(fh == NULL){
        return ENOMEM;
    }
    curthread->t_ftable[i]=fh;
    *fd = i;
    set_current_fd(i,fh);;
    *fd=i;
    return 0;
}

int 
sys_close(int fd)
{
    if(!is_valid_file_descriptor(fd) || is_fh_null(fd)){
        return EBADF;
    }
    struct fhandle *fh = get_filehandle(fd);
    lock_acquire(fh->lk);
    set_current_fd(fd,NULL);
    if(fh->ref_count==1){
        vfs_close(fh->vn);
        lock_release(fh->lk);
        fhandle_destroy(fh);
    }
    else{
        fh->ref_count--;
        lock_release(fh->lk);
    }
    return 0;
}
/*
*
* Function sys_read
* Read reads up to buflen bytes from the file specified by fd.
*
*/
ssize_t
sys_read(int fd, void *buf, size_t buflen)
{

    // Sanity check
    if(!is_valid_file_descriptor(fd) || is_fh_null(fd)){
        return EBADF;
    }

    struct fhandle *fh = get_filehandle(fd);
    struct uio user_io;
    struct iovec io_vec;
    int result;
    ssize_t byte_read_count = -1;

    lock_acquire(fh->lk);

    if(fh->permission_flags & O_RDONLY ||
       fh->permission_flags & O_RDWR)
    {
        uio_kinit(&io_vec, &user_io, buf, buflen, fh->offset,UIO_READ);
        user_io.uio_segflg = UIO_USERSPACE;
        user_io.uio_space = curthread->t_proc->p_addrspace;
        result = VOP_READ(fh->vn, &user_io);
        if(result){
            lock_release(fh->lk);
            return result;
        }
        byte_read_count = buflen - user_io.uio_resid;
        fh->offset += byte_read_count;
        lock_release(fh->lk);
    }
    else{
        lock_release(fh->lk);
        return EBADF;
    }
    return byte_read_count;
}