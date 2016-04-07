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

    spinlock_init(&coremap_spinlock);
    coremap = (struct coremap_entry*) PADDR_TO_KVADDR(first_address);
    free_address = first_address + total_num_pages * (sizeof(struct coremap_entry));

    // Iterate all kernel entries
    for(index = 0; index < (free_address / PAGE_SIZE); index += 1 ){
        struct coremap_entry new_cmap_entry;
        new_cmap_entry.is_fixed = true;
        new_cmap_entry.is_free = false;
        new_cmap_entry.is_dirty = false;
        new_cmap_entry.is_clean = false;
        new_cmap_entry.chunk_size = 0;
        coremap[index] = new_cmap_entry;
    }

    for(index = (free_address / PAGE_SIZE); index < (last_address / PAGE_SIZE); index += 1 ){
        struct coremap_entry new_cmap_entry;
        new_cmap_entry.is_fixed = false;
        new_cmap_entry.is_free = true;
        new_cmap_entry.is_dirty = false;
        new_cmap_entry.is_clean = false;
        new_cmap_entry.chunk_size = 0;
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
    // Refer PADDR_TO_KVADDR
    paddr_t physical_page_addr = addr - MIPS_KSEG0;
    uint32_t cmap_index = physical_page_addr / PAGE_SIZE;
    uint32_t loop_index, new_index;

    spinlock_acquire(&coremap_spinlock);
    struct coremap_entry cmap_entry = coremap[cmap_index];
    // Get the size of the chunk
    size_t chunk_size = cmap_entry.chunk_size;
    cmap_entry.chunk_size = 0;
    // Check if we are freeing a valid chunk
    KASSERT(chunk_size != 0);
    for(loop_index = 0; loop_index < chunk_size; loop_index += 1){
        new_index = cmap_index + loop_index;
        cmap_entry = coremap[new_index];
        cmap_entry.is_fixed = false;
        cmap_entry.is_free = true;
        cmap_entry.is_dirty = false;
        cmap_entry.is_clean = false;
        coremap[new_index] = cmap_entry;
    }
    spinlock_release(&coremap_spinlock);
};

unsigned int
coremap_used_bytes(void){
    unsigned int total_used_entries = 0, index;
    
    // TODO check if active waiting is costly
    // spinlock_acquire(&coremap_spinlock);
    for(index = 0; index < total_num_pages; index += 1){
        if(!coremap[index].is_free){
            total_used_entries++;
        }

    }    
    // spinlock_release(&coremap_spinlock);
    return total_used_entries * PAGE_SIZE;
};

void 
vm_tlbshootdown_all(void){

};
void
vm_tlbshootdown(const struct tlbshootdown *tlb){
    (void) tlb;
};



