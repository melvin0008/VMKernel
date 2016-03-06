/*
File Handle Definition
*/

#ifndef _FILE_HANDLE_H
#define _FILE_HANDLE_H

#include <types.h>
#include <synch.h>
#include <vnode.h>

struct fhandle
{
    char *fhandle_name;   //name for the file handle
    struct vnode *vn;     // vnode file object
    struct  lock *lk;    // lock for synchronization
    off_t offset;        //offset for the file
    int permission_flags;  // permissions defined in fcntl.h
    int *ref_count;       //ref count for parent child processes
};


struct fhandle *fhandle_create(const char *name); 
void fhandle_destroy(struct fhandle *);

#endif /* _FILE_HANDLE_H */