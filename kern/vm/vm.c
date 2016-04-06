// Initialization 

#include <types.h>
#include <spinlock.h>
#include <vm.h>



void 
init_coremap(){
    paddr_t last_address = ram_getsize();
    paddr_t first_address = ram_getfirstfree();
    paddr_t free_address;
    uint32_t index = 0;
    uint32_t total_num_pages = (last_address - first_address) / PAGE_SIZE;

    coremap = (struct coremap_entry*) PADDR_TO_KVADDR(first_address);
    free_address = first_address + total_num_pages * (sizeof(struct coremap_entry));

    // Iterate all kernel entries
    for(index = 0; index < (free_address / PAGE_SIZE); index += 1 ){
        struct coremap_entry new_cmap_entry;
        // Init fields
        new_cmap_entry.is_fixed = true;
        new_cmap_entry.is_free = false;
        new_cmap_entry.is_dirty = false;
        new_cmap_entry.is_clean = false;
        new_cmap_entry.chunk_size = 0;
        spinlock_init(new_cmap_entry.cmap_entry_spinlock);

        coremap[index] = new_cmap_entry;
    }

    for(index = (free_address / PAGE_SIZE); index < (last_address / PAGE_SIZE); index += 1 ){
        struct coremap_entry new_cmap_entry;
        // Init fields
        new_cmap_entry.is_fixed = false;
        new_cmap_entry.is_free = true;
        new_cmap_entry.is_dirty = false;
        new_cmap_entry.is_clean = false;
        new_cmap_entry.chunk_size = 0;
        spinlock_init(new_cmap_entry.cmap_entry_spinlock);

        coremap[index] = new_cmap_entry;
    }

}

void
vm_bootstrap(void){

};

int
vm_fault(int faulttype, vaddr_t faultaddress){
    (void) faulttype;
    (void) faultaddress;
    return 0;
};

vaddr_t
alloc_kpages(unsigned npages){
    (void) npages;
    return 0;
};

void
free_kpages(vaddr_t addr){
    (void) addr;
};

unsigned int
coremap_used_bytes(void){
    return 0;
};

void 
vm_tlbshootdown_all(void){

};
void
vm_tlbshootdown(const struct tlbshootdown *tlb){
    (void) tlb;
};



