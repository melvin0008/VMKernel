// Initialization 

#include <types.h>
#include <lib.h>
#include <elf.h>
#include <spinlock.h>
#include <vm.h>
#include <kern/errno.h>
#include <proc.h>
#include <addrspace.h>
#include <current.h>
#include <spinlock.h>
#include <cpu.h>
#include <spl.h>
#include <machine/tlb.h>
#include <swap_table_entry.h>
#include <clock.h>


static struct spinlock tlb_spinlock;
static uint32_t total_num_pages;
static uint32_t first_free_page;

static void set_cmap_entry(struct coremap_entry *cmap, bool is_fixed , bool is_free, bool is_dirty, bool is_clean , size_t chunk_size, struct addrspace *as, vaddr_t va){
    struct coremap_entry cmap_copy;
    cmap_copy = *cmap;
    cmap_copy.is_fixed = is_fixed;
    cmap_copy.is_free = is_free;
    cmap_copy.is_dirty = is_dirty;
    cmap_copy.is_clean = is_clean;
    cmap_copy.chunk_size = chunk_size;
    cmap_copy.as = as;
    cmap_copy.va = va & PAGE_FRAME;


    *cmap = cmap_copy;
}

static void set_cmap_fixed(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    // KASSERT(chunk_size!=0);
    set_cmap_entry(cmap,true,false,false,false,chunk_size, as, va);
}

static void set_cmap_free(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,false,true,false,false,chunk_size, as, va);
}

static void set_cmap_dirty(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,false,false,true,false,chunk_size, as, va);
}

static void set_cmap_clean(struct coremap_entry *cmap){
    struct coremap_entry cmap_copy;
    cmap_copy = *cmap;
    cmap_copy.is_fixed = false;
    cmap_copy.is_free = false;
    cmap_copy.is_dirty = false;
    cmap_copy.is_clean = true;
    *cmap = cmap_copy;
}

static void invalidate_tlb_entry(vaddr_t va){
    spinlock_acquire(&tlb_spinlock);
    int spl = splhigh();
    int tlb_index = tlb_probe(va & PAGE_FRAME, 0);
    if(tlb_index >= 0){
        tlb_write(TLBHI_INVALID(tlb_index), TLBLO_INVALID(), tlb_index);
    }
    splx(spl);
    spinlock_release(&tlb_spinlock);
}

static void swapout_page(int cmap_index){
    // Acquire locks for cmap and page table
    // TODO LOCK FOR PTE???
    // spinlock_acquire(&coremap_spinlock);
    struct coremap_entry cmap_entry = coremap[cmap_index];
    // spinlock_release(&coremap_spinlock);
    // Shootdown tlb entry
    // Copy contents from mem to disk
    invalidate_tlb_entry(cmap_entry.va);
    memory_to_swapdisk(cmap_index);
    // Update pte
    
}

static void swapin_page(struct addrspace *as, vaddr_t va, struct page_table_entry *pte){
    
    // Allocate a physical page
    paddr_t physical_page_addr = page_alloc(as, va);
    pte->physical_page_number = physical_page_addr;
    // Find the swap slot using ste and
    // Copy stuff from disk to mem
    swapdisk_to_memory(pte, physical_page_addr);

    // Update the pte to indicate that the page is now in memory
}

static paddr_t find_continuous_block(bool is_swap, unsigned npages){
    paddr_t p = 0;
    uint32_t start_page = 0;
    uint32_t j;
    uint32_t i;
    bool found_section;
    bool condition_to_check;
    for(i = first_free_page; i<total_num_pages; i++ ){
        found_section = false;
        start_page = i;
        if( i+npages> total_num_pages){
            // spinlock_release(&coremap_spinlock);
            // return 0;
            break;
        }
        for( j = start_page; j < start_page+npages; j++ ){

            if(is_swap){
                condition_to_check = coremap[j].is_free || coremap[j].is_dirty;
            }
            else{
                condition_to_check = coremap[j].is_free;
            }

            if(condition_to_check){
                found_section = true;
                // i++;
            }
            else{
                found_section = false;
                break;
            }


        }
        if(j>=total_num_pages){
            // spinlock_release(&coremap_spinlock);
            // return 0;
            found_section = false;
            break;
        }
        if(found_section){
            if(is_swap && coremap[start_page].is_dirty){
                // SWAP
                set_cmap_clean(&coremap[start_page]);
                swapout_page(start_page);
            }
            set_cmap_fixed(&coremap[start_page],npages, NULL, PADDR_TO_KVADDR(start_page * PAGE_SIZE));

            for (uint32_t l = start_page + 1; l < npages + start_page; l++){
                if(is_swap && coremap[l].is_dirty){
                    // SWAP
                    set_cmap_clean(&coremap[l]);
                    swapout_page(l);
                }
                set_cmap_fixed(&coremap[l], 0, NULL, PADDR_TO_KVADDR(l * PAGE_SIZE));
            }
            p = start_page * PAGE_SIZE;
            break;
        }
    }
    return p;
}

static uint32_t get_victim_index(){
    // KASSERT(spinlock_do_i_hold(&coremap_spinlock));
    uint32_t i;
    uint32_t rstart = first_free_page + (random()%(total_num_pages-first_free_page));
    // kprintf("Rstart %d first free%d",rstart,first_free_page);
    for(i = rstart; i < total_num_pages; i++){
        if(coremap[i].is_dirty ){
            return i;
        }
    }
    for(i = first_free_page; i < rstart; i++){
        if(coremap[i].is_dirty){
            return i;
        }
    }
    
    return 0;

}



//Ref :
//http://jhshi.me/2012/04/24/os161-coremap/index.html
void 
init_coremap(){
    paddr_t last_address = ram_getsize();
    paddr_t first_address = ram_getfirstfree();
    uint32_t index;
    paddr_t free_address;
    total_num_pages = last_address/ PAGE_SIZE;
    spinlock_init(&coremap_spinlock);
    spinlock_init(&tlb_spinlock);
    coremap = (struct coremap_entry*) PADDR_TO_KVADDR(first_address);
    free_address = first_address + total_num_pages * (sizeof(struct coremap_entry));
    first_free_page = free_address / PAGE_SIZE + 1;
    // Iterate all kernel entries
    for(index = 0; index < first_free_page ; index ++ ){
        set_cmap_fixed(&coremap[index], 1, NULL, PADDR_TO_KVADDR(index * PAGE_SIZE));
    }
    for(index = first_free_page ; index < total_num_pages; index ++ ){
        set_cmap_free(&coremap[index], 0, NULL, 0);
    }
}

void
vm_bootstrap(void){
    // Do nothing now
};

//Ref :
//http://jhshi.me/2012/04/24/os161-physical-page-management/index.html
vaddr_t
alloc_kpages(unsigned npages){
    paddr_t p;
    KASSERT(npages>0);
    if(npages>1){
        //Multiple Pages
        // lock_acquire()
        spinlock_acquire(&coremap_spinlock);
        
        p = find_continuous_block(false, npages);


        if(p == 0){
            //evict
            //swapout
            // panic("Should never get here");
            p = find_continuous_block(true, npages);
            if(p == 0){
                panic("Can't allocate continuous pages even with swapping enabled");
            }

        }

        
        spinlock_release(&coremap_spinlock);
        bzero((void *)PADDR_TO_KVADDR(p), npages * PAGE_SIZE);
    }
    else{
        //Single Page

        uint32_t i;
        spinlock_acquire(&coremap_spinlock);
        for( i = first_free_page; i < total_num_pages; i++){
            if(coremap[i].is_free){
                set_cmap_fixed(&coremap[i], 1, NULL, PADDR_TO_KVADDR(i * PAGE_SIZE));
                p = i * PAGE_SIZE;
                bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
                break;
            }
        }

        if(i>=total_num_pages){
            // spinlock_release(&coremap_spinlock);
            // return 0;
            i = get_victim_index();
            if(i == 0){
                spinlock_release(&coremap_spinlock);

                // spinlock_release(&coremap_spinlock);
                return 0;
            }
            set_cmap_clean(&coremap[i]);
            swapout_page(i);
            // Update the new owner info
            set_cmap_fixed(&coremap[i], 1, NULL, PADDR_TO_KVADDR(i * PAGE_SIZE));
            p = i * PAGE_SIZE;
            bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
        }
        spinlock_release(&coremap_spinlock);
    }

    return PADDR_TO_KVADDR(p);
};


paddr_t page_alloc(struct addrspace *as, vaddr_t va){
    KASSERT(va != 0);
    uint32_t i;
    paddr_t p;
    spinlock_acquire(&coremap_spinlock);
    for( i = first_free_page; i < total_num_pages; i++){
        if(coremap[i].is_free){

            struct timespec time;
            gettime(&time);
            coremap[i].sec = time.tv_sec;
            coremap[i].nanosec = time.tv_nsec;

            p = i * PAGE_SIZE;
            set_cmap_dirty(&coremap[i], 1, as, va);
            bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
            break;
        }
    }

    // Check if we don't have ane free page left
    if(i>=total_num_pages){
        // We need to swap!

        // panic("No Page left");
        // return 0;
        
        i = get_victim_index();
        if(i == 0){
            spinlock_release(&coremap_spinlock);
            return 0;
        }
        
        set_cmap_clean(&coremap[i]);
        swapout_page(i);
        // Update the new owner info
        struct timespec time;
        gettime(&time);
        coremap[i].sec = time.tv_sec;
        coremap[i].nanosec = time.tv_nsec;
        p = i * PAGE_SIZE;
        set_cmap_dirty(&coremap[i], 1, as, va);
        bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
    }
    spinlock_release(&coremap_spinlock);
    return p;
};


void
free_pages(paddr_t physical_page_addr, vaddr_t v_addr){
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
        set_cmap_free(&coremap[cmap_index], 0, NULL, 0);
    }
    spinlock_release(&coremap_spinlock);

    invalidate_tlb_entry(v_addr);
};

void
free_kpages(vaddr_t addr){
    free_pages(addr - MIPS_KSEG0, addr);
};

void page_free(paddr_t physical_page_addr, vaddr_t v_addr){

    KASSERT( physical_page_addr % PAGE_SIZE == 0);
    // Refer PADDR_TO_KVADDR
    uint32_t cmap_index = physical_page_addr / PAGE_SIZE;
    // uint32_t last_index;
        
    spinlock_acquire(&coremap_spinlock);
    // Get the size of the chunk
    // size_t chunk_size = coremap[cmap_index].chunk_size;
    // Check if we are freeing a valid chunk
    // KASSERT(chunk_size != 0);
    
    // last_index = (cmap_index+chunk_size);
    // for(; cmap_index <last_index ; cmap_index ++){
        set_cmap_free(&coremap[cmap_index], 0, NULL, 0);
    // }
    spinlock_release(&coremap_spinlock);
    // free_pages(addr);
    // TODO check if the page is mapped to any file
    // or basically see if it is a userspace page and 
    // based on that swap it to disk / unmap it.
    // TODO Also shootdown TLB entry if needed

    invalidate_tlb_entry(v_addr);

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

    /* Disable interrupts on this CPU while frobbing the TLB. */
    spinlock_acquire(&tlb_spinlock);
    int i, spl;
    spl = splhigh();
    for (i=0; i<NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
    splx(spl);
    spinlock_release(&tlb_spinlock);
        
};
void
vm_tlbshootdown(const struct tlbshootdown *tlb){
    (void) tlb;
};

static bool
is_addr_in_stack_or_heap(struct addrspace *as, vaddr_t addr){
    if((addr >= as->heap_start && addr < as->heap_end) ||
        (addr >= as->stack_end&& addr < USERSTACK))
        return true;
    return false;
}

static bool
has_permission(int faulttype, struct page_table_entry *pte){
    if(faulttype == VM_FAULT_READ){
        if(pte->permission & PF_R){
            return true;
        }
    }
    else if (faulttype == VM_FAULT_WRITE || faulttype == VM_FAULT_READONLY){
     if(pte->permission & PF_W){
            return true;
        }   
    }

    panic("ACCESS VIOLATION");
    return false;
}

/// Reference http://jhshi.me/2012/04/27/os161-tlb-miss-and-page-fault/index.html

int
vm_fault(int faulttype, vaddr_t faultaddress){

    faultaddress &= PAGE_FRAME;

    if (curproc == NULL) {
        return EFAULT;
    }

    struct addrspace* as = proc_getas();    
    if(as == NULL){
        return EFAULT;
    }
    // Check if the address is a valid userspace address
    struct addrspace_region *addr_region = get_region_for(as, faultaddress);
    struct page_table_entry *pte;
    bool is_stack_or_heap = is_addr_in_stack_or_heap(as, faultaddress);
    int permission, tlb_index, spl;
    if(addr_region==NULL && !is_stack_or_heap){
            return EFAULT;
    }
    if(addr_region == NULL){
        permission = PF_R | PF_W;
    }
    else{
        permission = addr_region->permission;
    }
    lock_acquire(page_lock);
    pte = search_pte(as, faultaddress);
    switch(faulttype){
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
            // 
            if(pte == NULL){
                // Allocate a new page (handled in add_pte)
                paddr_t new_paddr = page_alloc(as, faultaddress);
                pte = add_pte(as, faultaddress, new_paddr,permission);
                if(pte == NULL){
                    lock_release(page_lock);
                    
                    return ENOMEM;
                }
            }
            else{
                // Check if user has proper permission
                if(!has_permission(faulttype,pte)){
                    lock_release(page_lock);
                    return EFAULT;
                
                }
            }
            // lock_acquire(pte->pte_lock);
            if(pte->state == IN_DISK){
                swapin_page(as, faultaddress, pte);                
            }
            // lock_release(pte->pte_lock);

            // Add entry to tlb
            spinlock_acquire(&tlb_spinlock);
            spl = splhigh();
            tlb_index = tlb_probe(faultaddress,0);
            if(tlb_index >= 0){
                // TLB fault
                splx(spl);
                panic("Duplicate tlb entry");
            }
            int random_entry_lo;
            random_entry_lo = pte->physical_page_number;
            random_entry_lo = random_entry_lo | ((pte->permission & PF_W) | 1) << 9;
            tlb_random((uint32_t) faultaddress, random_entry_lo);
            splx(spl);
            spinlock_release(&tlb_spinlock);
            // Now we know that there is an entry in the TLB
            break;
        case VM_FAULT_READONLY:
            KASSERT(pte!=NULL);
            if(!has_permission(faulttype,pte)){
                    lock_release(page_lock);
                    return EFAULT;
                
                }

            // if(pte->state == IN_DISK){
            //     swapin_page(as, faultaddress, pte);                
            // }


            spinlock_acquire(&tlb_spinlock);
            spl = splhigh();
            tlb_index = tlb_probe(faultaddress,0);
            KASSERT(tlb_index>=0);
            uint32_t entry_hi, entry_lo;
            tlb_read(&entry_hi, &entry_lo, tlb_index);
            entry_lo |= TLBLO_DIRTY;
            tlb_write(entry_hi, entry_lo, tlb_index);
            splx(spl);
            spinlock_release(&tlb_spinlock);
        break;
        default:
        return EFAULT;
    }
    lock_release(page_lock);
    return 0;
};