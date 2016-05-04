#include <types.h>
#include <lib.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/fcntl.h>
#include <uio.h>
#include <vm.h>
#include <synch.h>
#include <bitmap.h>
#include <swap_table_entry.h>

// #include <addrspace.h>

static char swapdisk_buffer[PAGE_SIZE];

static struct lock *swap_vnode_lock;

void swap_disk_init(){
    int err;
    err = vfs_open((char *)"lhd0raw:",O_RDWR,0,&swap_vn);
    (void) err;
    // KASSERT(err == 0);
    swap_vnode_lock = lock_create("swap_node");
    swap_bitmap = bitmap_create(MAX_SWAP_TABLE_ENTIRES);
    page_lock = lock_create("page_lock");
}

unsigned get_clear_bit(){
    unsigned i;
    int err = bitmap_alloc(swap_bitmap, &i);
    if(err){
        panic("No place in swap Disk!!! Report to Discourse :p");
    }
    return i;
}

int copy_swapdisk(int old_disk_position){
    struct uio user_io;
    struct iovec io_vec;
    lock_acquire(swap_vnode_lock);
    KASSERT(old_disk_position!=-1);
    bzero((void *)swapdisk_buffer, PAGE_SIZE);
    int new_disk_position = get_clear_bit();
    uio_kinit(&io_vec, &user_io, swapdisk_buffer, PAGE_SIZE, old_disk_position * PAGE_SIZE,UIO_READ);
    int err = VOP_READ(swap_vn,&user_io);
    if(err) panic("Can't Read . I have no idea what to do now");
    struct uio user_io1;
    struct iovec io_vec1; 
    uio_kinit(&io_vec1,&user_io1,swapdisk_buffer,PAGE_SIZE, new_disk_position * PAGE_SIZE,UIO_WRITE);
    err = VOP_WRITE(swap_vn,&user_io1);
    if(err) panic("Can't Read . I have no idea what to do now");
    lock_release(swap_vnode_lock);
    return new_disk_position;
}


void memory_to_swapdisk(int cmap_index){
    (void) cmap_index;
    //find coremap_entry using index
    //paddr=index*PAGESIZE
    //get pte
    //if not available add_pte
    //Write to disk using VOP_WRITE
    //bzero the newly created space in memory
    struct coremap_entry cmap = coremap[cmap_index];
    spinlock_release(&coremap_spinlock);
    KASSERT(cmap.as != NULL);
    KASSERT(cmap.va != 0);

    lock_acquire(swap_vnode_lock);
    struct page_table_entry *pte = search_pte(cmap.as, cmap.va);
    KASSERT(pte != NULL);

    paddr_t paddr = cmap_index * PAGE_SIZE;
    // lock_release(swap_vnode_lock);
    int disk_position = get_clear_bit();
    
    struct uio user_io;
    struct iovec io_vec;
    // lock_acquire(swap_vnode_lock);
    // lock_acquire(pte->pte_lock);

    pte->disk_position = disk_position;
    pte->state = IN_DISK;
    uio_kinit(&io_vec,&user_io,(void *) PADDR_TO_KVADDR(paddr),PAGE_SIZE, disk_position * PAGE_SIZE,UIO_WRITE);
    int err = VOP_WRITE(swap_vn,&user_io);
    if(err) panic("Can't Read . I have no idea what to do now");
    // lock_release(pte->pte_lock);
    lock_release(swap_vnode_lock);
    spinlock_acquire(&coremap_spinlock);


}

void swapdisk_to_memory(struct page_table_entry *pte, paddr_t paddr){
    //get_ste_position
    //paddr/PAGESIZE -> Coremap index
    //read from swapdisk using VOP_READ
    //Copy to Coremap index
    // lock_acquire(page_lock);
    lock_acquire(swap_vnode_lock);
    int disk_position = pte->disk_position;
    KASSERT(disk_position!=-1);
    struct uio user_io;
    struct iovec io_vec;
    uio_kinit(&io_vec, &user_io, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, disk_position * PAGE_SIZE,UIO_READ);
    int err = VOP_READ(swap_vn,&user_io);
    if(err) panic("Can't Read . I have no idea what to do now");
    bitmap_unmark(swap_bitmap, disk_position);
    pte->state = IN_MEM;
    lock_release(swap_vnode_lock);
    // lock_release(page_lock);
}

