#include <types.h>
#include <lib.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/fcntl.h>
#include <kern/stat.h>
#include <uio.h>
#include <vm.h>
#include <synch.h>
#include <bitmap.h>
#include <wchan.h>
#include <swap_table_entry.h>

static char swapdisk_buffer[PAGE_SIZE];

void swap_disk_init(){
    int err;

    err = vfs_open((char *)"lhd0raw:",O_RDWR,0,&swap_vn);
    swap_bitmap = bitmap_create(MAX_SWAP_TABLE_ENTIRES);
    swap_vnode_lock = lock_create("swap_node");
    clock_hand_lock = lock_create("clock-hand");
    tlb_wchan=wchan_create("TLB_WCHAN");
    if(!err){
        is_swapping = true;
    }
    else{
        is_swapping =false;
    }

}

unsigned get_clear_bit(){
    unsigned i;
    int err = bitmap_alloc(swap_bitmap, &i);
    if(err){
        panic("No place in swap Disk!!! Report to Discourse :p");
    }
    return i;
}

void memory_to_swapdisk(paddr_t paddr,int disk_position){
    //Write to disk using VOP_WRITE
    //bzero the newly created space in memory
    KASSERT(paddr!=0);    
    struct uio user_io;
    struct iovec io_vec;
    KASSERT(bitmap_isset(swap_bitmap, disk_position)!=0);
    uio_kinit(&io_vec,&user_io,(void *) PADDR_TO_KVADDR(paddr),PAGE_SIZE, disk_position * PAGE_SIZE,UIO_WRITE);
    int err = VOP_WRITE(swap_vn,&user_io);
    if(err) panic("Can't Read . I have no idea what to do now");
}

void swapdisk_to_memory(int disk_position, paddr_t paddr){
    struct uio user_io;
    struct iovec io_vec;
    
    KASSERT(bitmap_isset(swap_bitmap, disk_position)!=0);
    uio_kinit(&io_vec, &user_io, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, disk_position * PAGE_SIZE,UIO_READ);
    int err = VOP_READ(swap_vn,&user_io);
    if(err) panic("Can't Read . I have no idea what to do now");
}

void copy_swapdisk(int old_disk_position,int new_disk_position){
    struct uio user_io;
    struct iovec io_vec;
    KASSERT(old_disk_position!=-1);
    lock_acquire(swap_vnode_lock);
    uio_kinit(&io_vec, &user_io, swapdisk_buffer, PAGE_SIZE, old_disk_position * PAGE_SIZE,UIO_READ);
    int err = VOP_READ(swap_vn,&user_io);
    if(err) panic("Can't Read . I have no idea what to do now");
    struct uio user_io1;
    struct iovec io_vec1; 
    uio_kinit(&io_vec1,&user_io1,swapdisk_buffer,PAGE_SIZE, new_disk_position * PAGE_SIZE,UIO_WRITE);
    err = VOP_WRITE(swap_vn,&user_io1);
    if(err) panic("Can't Read . I have no idea what to do now");
    lock_release(swap_vnode_lock);
}