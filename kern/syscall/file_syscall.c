/*
* File Syscall Implementations
* Header file in file_syscall.h
*/

#include <types.h>
#include <vnode.h>
#include <kern/fcntl.h>
#include <kern/errno.h>
#include <kern/unistd.h>
#include <kern/filehandle.h>
#include <kern/seek.h>
#include <copyinout.h>
#include <uio.h>
#include <vfs.h>
#include <file_syscall.h>
#include <stat.h>
#include <limits.h>
#include <current.h>
#include <proc.h>


/*
* Reference:
* http://jhshi.me/2012/03/28/os161-arguments-passing-in-system-call/index.html
* http://jhshi.me/2012/03/14/os161-file-system-calls/index.html
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
    if(is_invalid_file_descriptor(i)){
        return EMFILE;
    }
    fh = fhandle_create((const char*)filename,vn,offset,flag);
    if(fh == NULL){
        return ENOMEM;
    }
    lock_acquire(fh->lk);
    set_current_fd(i,fh);;
    *fd=i;
    lock_release(fh->lk);
    return 0;
}

int 
sys_close(int fd)
{
    if(is_invalid_file_descriptor(fd) || is_fh_null(fd)){
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
* Reads up to buflen bytes from the file specified by fd.
*
*/

int
sys_read(int fd, void *buf, size_t buflen, ssize_t *retval){
        // Sanity check
    if(is_invalid_file_descriptor(fd) || is_fh_null(fd)){
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
        *retval=byte_read_count;
        lock_release(fh->lk);
    }
    else{
        *retval=byte_read_count;
        lock_release(fh->lk);
        return EBADF;
    }
    return 0;
}

/*
*
* Function sys_write
* Writes up to buflen bytes from the file specified by fd.
*
*/
int
sys_write(int fd, void *buf, size_t buflen, ssize_t *retval){
        // Sanity check
    if(is_invalid_file_descriptor(fd) || is_fh_null(fd)){
        return EBADF;
    }

    struct fhandle *fh = get_filehandle(fd);
    struct uio user_io;
    struct iovec io_vec;
    int result;
    lock_acquire(fh->lk);
    ssize_t byte_write_count = -1;

    if(fh->permission_flags & O_WRONLY ||
       fh->permission_flags & O_RDWR)
    {
        uio_kinit(&io_vec, &user_io, buf, buflen, fh->offset,UIO_WRITE);
        user_io.uio_segflg = UIO_USERSPACE;
        user_io.uio_space = curthread->t_proc->p_addrspace;
        result = VOP_WRITE(fh->vn, &user_io);
        if(result){
            lock_release(fh->lk);
            return result;
        }
        byte_write_count = buflen - user_io.uio_resid;
        fh->offset += byte_write_count;
        *retval=byte_write_count;
        lock_release(fh->lk);
    }
    else{
        *retval=byte_write_count;
        lock_release(fh->lk);
        return EBADF;
    }
    
    return 0;
}


/*
*
* Function sys_dups
* Duplicates file handles
*
*/
int
sys_dup2(int oldfd, int newfd){
    if(oldfd==newfd){
        return 0;
    }
    if(is_invalid_file_descriptor(oldfd) || is_invalid_file_descriptor(newfd) || is_fh_null(oldfd)){
        return EBADF;
    }
    struct fhandle *old_fh= get_filehandle(oldfd);
    lock_acquire(old_fh->lk);
    if(!is_fh_null(newfd)){
        int err = sys_close(newfd);
        if(err){
            return err;
        }
    }
    curthread->t_ftable[oldfd]++;
    set_current_fd(newfd,old_fh);
    lock_release(old_fh->lk);
    return 0;

}

/*
*
* Function sys_chdir
* Change directory to the new pathname 
*/

int
sys_chdir(const char *pathname){

    char kernel_buffer[NAME_MAX]; 
    int err,result;
    size_t actual;

    err = copyinstr((const_userptr_t) pathname, kernel_buffer, NAME_MAX, &actual);
    if (err != 0){ 
        return err; 
    } 
    result = vfs_chdir(kernel_buffer);
    if (result) {
        return result;
    }
    return 0;
}


int
sys__getcwd(char *buf, size_t buflen, size_t *retval){
    
    if(buf==NULL || (userptr_t) buf== (userptr_t)INVAL_ADDR){
        return EFAULT;
    }

    char kernel_buffer[NAME_MAX]; 
    int err;
    size_t actual;
    err = copyinstr((const_userptr_t) buf, kernel_buffer, NAME_MAX, &actual);
    if (err != 0){ 
        return err;
    } 
    struct uio user_io;
    struct iovec io_vec;
    uio_kinit(&io_vec, &user_io, kernel_buffer, buflen, 0,UIO_READ);
    user_io.uio_segflg = UIO_USERSPACE;
    user_io.uio_space = curthread->t_proc->p_addrspace; 
    err=vfs_getcwd(&user_io);
    if(err){
        return err;
    }
    *retval=buflen - user_io.uio_resid;
    return 0;
}

int
sys_lseek (int fd, off_t pos, int whence,off_t *retval){
    if(is_invalid_file_descriptor(fd) || is_fh_null(fd)){
        return EBADF;
    }
    if(whence != SEEK_SET || whence != SEEK_CUR || whence != SEEK_END){
        return EINVAL;
    }
    if(fd == STDIN_FILENO && fd == STDOUT_FILENO && fd == STDERR_FILENO ){
        return ESPIPE;
    }
    //Make sure what they mean by resulting seek position
    // if(pos<0){
    //     return EINVAL;
    // }
    struct fhandle *fh = get_filehandle(fd);
    int result,err;
    err=VOP_ISSEEKABLE(fh->vn);
    if(err){
        return ESPIPE;
    }
    off_t new_offset = 0;
    struct stat stat_offset;
    switch(whence){
        case SEEK_SET:
            new_offset = pos;
        break;
        case SEEK_CUR:
            new_offset = fh->offset + pos;
        break;
        case SEEK_END:
            result = VOP_STAT(fh->vn,&stat_offset);
            if (result) {
               return result;
            }
            new_offset = stat_offset.st_size + pos;
        break;
    }
    if(new_offset<0){
        return EINVAL;
    }
    *retval=new_offset;
    return 0;
}