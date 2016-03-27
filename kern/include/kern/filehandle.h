/*
File Handle Definition
*/

#ifndef _FILE_HANDLE_H
#define _FILE_HANDLE_H

#include <types.h>
#include <synch.h>
#include <vnode.h>

/*Didn't know where else to add this*/
#define KERN_PTR    ((void *)0x80000000)    /* addr within kernel */
#define INVAL_PTR ((void *)0x40000000)

struct fhandle
{
    char *name;   //name for the file handle
    struct vnode *vn;     // vnode file object
    struct  lock *lk;    // lock for synchronization
    off_t offset;        //offset for the file
    int permission_flags;  // permissions defined in fcntl.h
    int ref_count;       //ref count for parent child processes
};


struct fhandle *fhandle_create(const char *name, struct vnode *vn, off_t offset, int permission_flags); 
struct fhandle *get_filehandle(int fd);
void set_current_fd(int fd, struct fhandle *fh);
bool is_fh_null(int fd);
bool is_invalid_file_descriptor(int fd);
void fhandle_destroy(struct fhandle *);

#endif /* _FILE_HANDLE_H */