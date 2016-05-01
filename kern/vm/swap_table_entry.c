#include <types.h>
#include <lib.h>
#include <vfs.h>
#include <vnode.h>
#include <kern/fcntl.h>
#include <swap_table_entry.h>

void swap_disk_init(){
    int err;
    err = vfs_open((char *)"lhd0raw:",O_RDWR,0,&swap_vn);
    KASSERT(err == 0);
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

void add_ste(struct addrspace *as, vaddr_t va){
    for (int i = 0 ; i < MAX_SWAP_TABLE_ENTIRES; i++ ){
        if(swap_table[i] == NULL){
            swap_table[i] = create_ste(as,va);
        }
    }
}


int get_ste_position(struct addrspace *as, vaddr_t va){
    
    //TODO: Explore bitmap.h to look for an elegant way to do this shit!
    // Search the swap table
    for (int i = 0 ; i < MAX_SWAP_TABLE_ENTIRES; i++ ){
        if(swap_table[i]->as == as && swap_table[i]->va == va){
            return i;
        }
    }
    // Allocate a new slot if full
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

}

void swapdisk_to_memory(struct addrspace *as, vaddr_t va,paddr_t paddr){
    (void) as;
    (void) va;
    (void) paddr;
    //get_ste_position
    //paddr/PAGESIZE -> Coremap index
    //read from swapdisk using VOP_READ
    //Copy to Coremap index
}

void destroy_ste(struct swap_table_entry *ste){
    kfree(ste);
}
