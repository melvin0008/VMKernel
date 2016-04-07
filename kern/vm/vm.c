// Initialization 

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <vm.h>
#include <current.h>
#include <spinlock.h>
#include <cpu.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;
static uint32_t total_num_pages;
static paddr_t free_address;
static bool is_bootstrapped = false;
void 
init_coremap(){
    spinlock_init(&stealmem_lock);
    paddr_t last_address = ram_getsize();
    paddr_t first_address = ram_getfirstfree();
    uint32_t index = 0;
    total_num_pages = (last_address - first_address) / PAGE_SIZE;

    spinlock_init(&coremap_spinlock);
    coremap = (struct coremap_entry*) PADDR_TO_KVADDR(first_address);
    free_address = first_address + total_num_pages * (sizeof(struct coremap_entry));

    // Iterate all kernel entries
    for(index = 0; index < (free_address / PAGE_SIZE); index ++ ){
        struct coremap_entry new_cmap_entry;
        new_cmap_entry.is_fixed = true;
        new_cmap_entry.is_free = false;
        new_cmap_entry.is_dirty = false;
        new_cmap_entry.is_clean = false;
        new_cmap_entry.chunk_size = 0;
        coremap[index] = new_cmap_entry;
    }

    for(index = (free_address / PAGE_SIZE); index < (last_address / PAGE_SIZE); index ++ ){
        struct coremap_entry new_cmap_entry;
        new_cmap_entry.is_fixed = false;
        new_cmap_entry.is_free = true;
        new_cmap_entry.is_dirty = false;
        new_cmap_entry.is_clean = false;
        new_cmap_entry.chunk_size = 0;
        coremap[index] = new_cmap_entry;
    }
    is_bootstrapped =true;
}

void
vm_bootstrap(void){

};

static
paddr_t
getppages(unsigned long npages)
{
    paddr_t addr;

    spinlock_acquire(&stealmem_lock);

    addr = ram_stealmem(npages);

    spinlock_release(&stealmem_lock);
    return addr;
}


int
vm_fault(int faulttype, vaddr_t faultaddress){
    (void) faulttype;
    (void) faultaddress;
    return 0;
};

vaddr_t
alloc_kpages(unsigned npages){
    paddr_t p;
    if(!is_bootstrapped){
        p = getppages(npages);
    }
    else{
        KASSERT(npages>0);
        // if(npages>1){
            //Multiple Pages
            uint32_t start_page = 0;
            for(uint32_t i = 0; i<total_num_pages; i++ ){
                bool found_section = false;
                uint32_t start_page = i;
                if( i+npages> total_num_pages){
                    return 0;
                }
                for(uint32_t j = i; j < i+npages; j++ ){
                    if(!coremap[i].is_fixed && (coremap[i].is_free || coremap[i].is_clean || coremap[i].is_dirty)){
                        found_section = true;
                        i++;
                    }
                    else{
                        found_section = false;
                        break;
                    }
                }
                if(i>=total_num_pages){
                    return 0;
                }
                if(found_section){
                    coremap[i].is_fixed = true;
                    coremap[i].is_free  = false;
                    coremap[i].is_dirty = false;
                    coremap[i].is_clean = false;
                    coremap[i].chunk_size = i - start_page;
                    break;
                }
                p = start_page * PAGE_SIZE;
            }
            for (uint32_t i = start_page+1; i < npages+start_page; i++){
                coremap[i].is_fixed = true;
                coremap[i].is_free  = false;
                coremap[i].is_dirty = false;
                coremap[i].chunk_size = -1;
                coremap[i].is_clean = false;
            }
        // }
        // else{
        //     //Single Page
        //     for(int i = (free_address/PAGE_SIZE); i<total_num_pages; i++){
        //         if(!coremap[i].is_fixed && (coremap[i].is_free || coremap[i].is_clean || coremap[i].is_dirty)){
        //             coremap[i].is_fixed = true;
        //             coremap[i].is_free  = false;
        //             coremap[i].is_dirty = false;
        //             coremap[i].is_clean = false;
        //             coremap[i].chunk_size = 1;
        //         }
        //     }
        // }
    }
    return PADDR_TO_KVADDR(p);
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



