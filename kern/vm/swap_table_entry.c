#include <types.h>
#include <lib.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/fcntl.h>
#include <swap_table_entry.h>
#include <vm.h>
#include <uio.h>
#include <synch.h>

static struct lock *swap_vnode_lock;

void swap_disk_init(){
    int err;
    err = vfs_open((char *)"lhd0raw:",O_RDWR,0,&swap_vn);
    (void) err;
    // KASSERT(err == 0);
    swap_vnode_lock = lock_create("swap_node");
}


struct swap_table_entry *create_ste(struct addrspace *as, vaddr_t va){
    struct swap_table_entry *ste = kmalloc(sizeof(*ste));
    if (ste == NULL) {
        return NULL;
    };
    ste->as = as;
    ste->va = va;
    return ste;
}

int add_ste(struct addrspace *as, vaddr_t va){
    for (int i = 0 ; i < MAX_SWAP_TABLE_ENTIRES; i++ ){
        if(swap_table[i] == NULL){
            swap_table[i] = create_ste(as,va);
            return i;
        }
    }
    panic("No place in swap Disk!!! Report to Discourse :p");
}


int get_ste_position(struct addrspace *as, vaddr_t va){
    
    //TODO: Explore bitmap.h to look for an elegant way to do this shit!
    // Search the swap table
    for (int i = 0 ; i < MAX_SWAP_TABLE_ENTIRES; i++ ){
        if(swap_table[i] == NULL) break;
        if(swap_table[i]->as == as && swap_table[i]->va == va){
            return i;
        }
    }
    return -1;
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
    paddr_t paddr = cmap_index * PAGE_SIZE;

    int disk_position = get_ste_position(cmap.as,cmap.va);
    if(disk_position == -1){
        disk_position = add_ste(cmap.as,cmap.va);
    }
    struct uio user_io;
    struct iovec io_vec;
    lock_acquire(swap_vnode_lock);
    uio_kinit(&io_vec,&user_io,(void *) PADDR_TO_KVADDR(paddr),PAGE_SIZE, disk_position * PAGE_SIZE,UIO_WRITE);
    int err = VOP_WRITE(swap_vn,&user_io);
    lock_release(swap_vnode_lock);
    if(err) panic("Can't Read . I have no idea what to do now");

}

void swapdisk_to_memory(struct addrspace *as, vaddr_t va,paddr_t paddr){
    (void) as;
    (void) va;
    (void) paddr;
    //get_ste_position
    //paddr/PAGESIZE -> Coremap index
    //read from swapdisk using VOP_READ
    //Copy to Coremap index
    int disk_position = get_ste_position(as,va);
    // int coremap_index = paddr / PAGE_SIZE;
    struct uio user_io;
    struct iovec io_vec;
    lock_acquire(swap_vnode_lock);
    uio_kinit(&io_vec, &user_io, (void *) PADDR_TO_KVADDR(paddr), PAGE_SIZE, disk_position * PAGE_SIZE,UIO_READ);
    int err = VOP_READ(swap_vn,&user_io);
    lock_release(swap_vnode_lock);
    if(err) panic("Can't Read . I have no idea what to do now");
}

void destroy_ste(struct swap_table_entry *ste){
    kfree(ste);
}
