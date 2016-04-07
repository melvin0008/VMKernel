// Initialization 

#include <types.h>
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

    coremap = (struct coremap_entry*) PADDR_TO_KVADDR(first_address);
    free_address = first_address + total_num_pages * (sizeof(struct coremap_entry));

    // Iterate all kernel entries
    for(index = 0; index < (free_address / PAGE_SIZE); index ++ ){
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

    for(index = (free_address / PAGE_SIZE); index < (last_address / PAGE_SIZE); index ++ ){
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



