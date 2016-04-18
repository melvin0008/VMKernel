// Initialization 

#include <types.h>
#include <lib.h>
#include <spinlock.h>
#include <vm.h>
#include <kern/errno.h>
#include <proc.h>
#include <addrspace.h>
#include <current.h>
#include <spinlock.h>
#include <cpu.h>

static struct spinlock coremap_spinlock;
static uint32_t total_num_pages;
static paddr_t free_address;
static uint32_t first_free_page;

static void set_cmap_entry(struct coremap_entry *cmap, bool is_fixed , bool is_free, bool is_dirty, bool is_clean , size_t chunk_size){
    struct coremap_entry cmap_copy;
    cmap_copy = *cmap;
    cmap_copy.is_fixed = is_fixed;
    cmap_copy.is_free = is_free;
    cmap_copy.is_dirty = is_dirty;
    cmap_copy.is_clean = is_clean;
    cmap_copy.chunk_size = chunk_size;
    *cmap = cmap_copy;
}

static void set_cmap_fixed(struct coremap_entry *cmap , size_t chunk_size){
    set_cmap_entry(cmap,true,false,false,false,chunk_size);
}

static void set_cmap_free(struct coremap_entry *cmap , size_t chunk_size){
    set_cmap_entry(cmap,false,true,false,false,chunk_size);
}

static void set_cmap_dirty(struct coremap_entry *cmap , size_t chunk_size){
    set_cmap_entry(cmap,false,false,true,false,chunk_size);
}

// static void set_cmap_clean(struct coremap_entry *cmap , size_t chunk_size){
//     set_cmap_entry(&cmap,false,false,false,true,chunk_size);
// }

//Ref :
//http://jhshi.me/2012/04/24/os161-coremap/index.html
void 
init_coremap(){
    paddr_t last_address = ram_getsize();
    paddr_t first_address = ram_getfirstfree();
    uint32_t index ;
    total_num_pages = last_address/ PAGE_SIZE;
    spinlock_init(&coremap_spinlock);
    coremap = (struct coremap_entry*) PADDR_TO_KVADDR(first_address);
    free_address = first_address + total_num_pages * (sizeof(struct coremap_entry));
    first_free_page = free_address / PAGE_SIZE + 1;
    // Iterate all kernel entries
    for(index = 0; index < first_free_page ; index ++ ){
        set_cmap_fixed(&coremap[index],0);
    }
    for(index = first_free_page ; index < total_num_pages; index ++ ){
        set_cmap_free(&coremap[index],0);
    }
}

void
vm_bootstrap(void){
    // Do nothing now
};


int
vm_fault(int faulttype, vaddr_t faultaddress){
    struct addrspace* as = proc_getas();    
    // Check if the address is a valid userspace address
    struct addrspace_region *addr_region = get_region_for(as, faultaddress);
    if(addr_region == NULL){
        return EFAULT;
    }

    switch(faulttype){
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
        case VM_FAULT_READONLY:
        break;
        default:
        return EFAULT;
    }

    return 0;
};

//Ref :
//http://jhshi.me/2012/04/24/os161-physical-page-management/index.html
vaddr_t
alloc_kpages(unsigned npages){
    paddr_t p;
    KASSERT(npages>0);
    if(npages>1){
        //Multiple Pages
        spinlock_acquire(&coremap_spinlock);
        uint32_t start_page = 0;
        uint32_t j;
        for(uint32_t i = first_free_page; i<total_num_pages; i++ ){
            bool found_section = false;
            start_page = i;
            if( i+npages> total_num_pages){
                spinlock_release(&coremap_spinlock);
                return 0;
            }
            for( j = start_page; j < start_page+npages; j++ ){
                if(!coremap[j].is_fixed){
                    found_section = true;
                    // i++;
                }
                else{
                    found_section = false;
                    break;
                }
            }
            if(j>=total_num_pages){
                spinlock_release(&coremap_spinlock);
                return 0;
            }
            if(found_section){
                set_cmap_fixed(&coremap[start_page],npages);
                break;
            }
        }
        for (uint32_t i = start_page+1; i < npages+start_page; i++){
            set_cmap_fixed(&coremap[i],0);
        }
        p = start_page * PAGE_SIZE;
        spinlock_release(&coremap_spinlock);
        bzero((void *)PADDR_TO_KVADDR(p), npages * PAGE_SIZE);
    }
    else{
        //Single Page

        uint32_t i;
        spinlock_acquire(&coremap_spinlock);
        for( i = first_free_page; i < total_num_pages; i++){
            if(!coremap[i].is_fixed){
                set_cmap_fixed(&coremap[i],1);
                p = i * PAGE_SIZE;
                bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
                break;
            }
        }
        if(i>=total_num_pages){
            spinlock_release(&coremap_spinlock);
            return 0;
        }
        spinlock_release(&coremap_spinlock);
    }

    return PADDR_TO_KVADDR(p);
};

void
free_pages(paddr_t physical_page_addr){
    KASSERT( physical_page_addr % PAGE_SIZE == 0);
    // Refer PADDR_TO_KVADDR
    uint32_t cmap_index = physical_page_addr / PAGE_SIZE;
    uint32_t last_index;
        
    spinlock_acquire(&coremap_spinlock);
    // Get the size of the chunk
    size_t chunk_size = coremap[cmap_index].chunk_size;
    // Check if we are freeing a valid chunk
    KASSERT(chunk_size != 0);
    
    last_index = (cmap_index+chunk_size);
    for(; cmap_index <last_index ; cmap_index ++){
        set_cmap_free(&coremap[cmap_index],0);
    }
    spinlock_release(&coremap_spinlock);
};

void
free_kpages(vaddr_t addr){
    free_pages(addr - MIPS_KSEG0);
};

void page_free(paddr_t addr){
    free_pages(addr);
    // TODO check if the page is mapped to any file
    // or basically see if it is a userspace page and 
    // based on that swap it to disk / unmap it.
    // TODO Also shootdown TLB entry if needed
};

paddr_t page_alloc(){
    uint32_t i;
    paddr_t p;
    spinlock_acquire(&coremap_spinlock);
    for( i = first_free_page; i < total_num_pages; i++){
        if(!coremap[i].is_fixed){
            set_cmap_dirty(&coremap[i],1);
            p = i * PAGE_SIZE;
            bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
            break;
        }
    }
    spinlock_release(&coremap_spinlock);
    if(i>=total_num_pages){
        return 0;
    }
    return p;
};




unsigned int
coremap_used_bytes(void){
    unsigned int total_used_entries = 0, index;
    
    // TODO check if active waiting is costly
    spinlock_acquire(&coremap_spinlock);
    for(index = first_free_page ; index < total_num_pages; index += 1){
        if(!coremap[index].is_free){
            total_used_entries++;
        }
    }    
    spinlock_release(&coremap_spinlock);
    return total_used_entries * PAGE_SIZE ;
};

void 
vm_tlbshootdown_all(void){

};
void
vm_tlbshootdown(const struct tlbshootdown *tlb){
    (void) tlb;
};



