/*
Implementation for interface filehandle.h
*/

#include <kern/filehandle.h>
#include <types.h>
#include <lib.h>
#include <current.h>

struct fhandle *
fhandle_create(const char *name, struct vnode *vn, off_t offset, int permission_flags){
    // Create a file handle
    struct fhandle *fh;
    fh = kmalloc(sizeof(*fh)); 
    if (fh == NULL) {
        kfree(fh);
        return NULL;
    }

    fh->name = kstrdup(name);
    if (fh->name == NULL) {
        kfree(fh);
        return NULL;
    }
    // We expect vnode to be passed 
    // As vnode creation requires knowledge of mode
    // And also the error handling is different
    fh->vn = vn;
    fh->lk = lock_create(name);
    fh->offset = offset;
    fh->permission_flags = permission_flags;
    fh->ref_count = 1;
    return fh;
}

void fhandle_destroy(struct fhandle *fh){
    kfree(fh->name);
    fh->ref_count = 0;
    // vnode_cleanup(fh->vn);
    lock_destroy(fh->lk);
    kfree(fh);
};

struct fhandle *get_filehandle(int fd){
    return curthread->t_ftable[fd];
}

void set_current_fd(int fd, struct fhandle *fh){
    curthread->t_ftable[fd]=fh;
}
bool is_fh_null(int fd){
    return curthread->t_ftable[fd]==NULL;
}
bool is_invalid_file_descriptor(int fd){
    return (fd<0 || fd>=OPEN_MAX);
}
