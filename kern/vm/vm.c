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
static struct spinlock swapout_spinlock;
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

static void set_busy(struct coremap_entry *cmap, bool is_busy){
    struct coremap_entry cmap_copy;
    cmap_copy = *cmap;
    cmap_copy.is_busy = is_busy;
    *cmap = cmap_copy;
}


static void set_cmap_fixed(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    // KASSERT(chunk_size!=0);
    set_cmap_entry(cmap,true,false,false,false,chunk_size, as, va);
    cmap->cmap_cpu = NULL;
}

static void set_cmap_free(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,false,true,false,false,chunk_size, as, va);
    cmap->cmap_cpu = NULL;
}

static void set_cmap_dirty(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,false,false,true,false,chunk_size, as, va);
    // cmap->cmap_cpu = curcpu;
}
static void set_cmap_clean(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,false,false,false,true,chunk_size, as, va);
    // cmap->cmap_cpu = curcpu;
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

    struct coremap_entry cmap_entry = coremap[cmap_index];
    struct page_table_entry *pte = search_pte(cmap_entry.as, cmap_entry.va);
    KASSERT(pte != NULL);
    // Shootdown tlb entry
    // Copy contents from mem to disk
    if(cmap_entry.cmap_cpu == curcpu){

        invalidate_tlb_entry(cmap_entry.va);
    }
    else{
        KASSERT(cmap_entry.cmap_cpu!=NULL);
        struct tlbshootdown tl;
        tl.as = cmap_entry.as;
        tl.va = cmap_entry.va;
        ipi_tlbshootdown(cmap_entry.cmap_cpu,(const struct tlbshootdown *) &tl);
    }
    // KASSERT(cmap_entry.cmap_cpu==NULL);
    cmap_entry.cmap_cpu = NULL;
    memory_to_swapdisk(cmap_index,pte);
    pte->state = IN_DISK;
    pte->physical_page_number = 0;
    // bzero((void*) PADDR_TO_KVADDR(cmap_index*PAGE_SIZE),PAGE_SIZE);    
}

static void swapin_page(struct addrspace *as, vaddr_t va, struct page_table_entry *pte){
    
    // Allocate a physical page
    paddr_t physical_page_addr = page_alloc(as, va & PAGE_FRAME);
    pte->physical_page_number = physical_page_addr;
    // Find the swap slot using ste and
    // Copy stuff from disk to mem
    uint32_t cmap_index = physical_page_addr/ PAGE_SIZE ;
    // set_cmap_clean(&coremap[physical_page_addr/PAGE_SIZE],1,as,va);
    KASSERT(!coremap[cmap_index].is_free && !coremap[cmap_index].is_fixed);
    set_busy(&coremap[cmap_index],1);
    swapdisk_to_memory(pte, physical_page_addr);
    set_busy(&coremap[cmap_index],0); 
    // set_cmap_dirty(&coremap[physical_page_addr/PAGE_SIZE],1,as,va);

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
                condition_to_check = !coremap[j].is_fixed && !coremap[j].is_busy; 
            }
            else{
                condition_to_check = coremap[j].is_free  && !coremap[j].is_busy;
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
            panic("oh no");
            found_section = false;
            break;
        }
        if(found_section){
            // KASSERT((coremap[start_page].is_dirty || coremap[i].is_clean)  && !coremap[i].is_busy)
            if(is_swap && (!coremap[start_page].is_fixed)  && !coremap[start_page].is_busy){
                set_busy(&coremap[start_page],1);
                swapout_page(start_page);
                set_busy(&coremap[start_page],0);
            }
            set_cmap_fixed(&coremap[start_page],npages, NULL, PADDR_TO_KVADDR(start_page * PAGE_SIZE));

            for (uint32_t l = start_page + 1; l < npages + start_page; l++){
                if(is_swap && (!coremap[start_page].is_fixed) && !coremap[l].is_busy){
                    set_busy(&coremap[l],1);
                    swapout_page(l);
                    set_busy(&coremap[l],0);
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
    // KASSERT(lock_do_i_hold(page_lock));
    // kprintf("Rstart %d first free%d",rstart,first_free_page);
    for(i = rstart; i < total_num_pages; i++){
        if(coremap[i].is_dirty && !coremap[i].is_busy ){
            // tlb_shootdown_all_cpus(coremap[i].as,coremap[i].va);
            // coremap[i].is_busy = 1;
            return i;
        }
    }
    for(i = first_free_page; i < rstart; i++){
        if(coremap[i].is_dirty && !coremap[i].is_busy){
            // tlb_shootdown_all_cpus(coremap[i].as,coremap[i].va);
            // coremap[i].is_busy = 1;
            return i;
        }
    }
     for(i = rstart; i < total_num_pages; i++){
        if(coremap[i].is_clean && !coremap[i].is_busy ){
            // tlb_shootdown_all_cpus(coremap[i].as,coremap[i].va);
            // coremap[i].is_busy = 1;
            return i;
        }
    }
    for(i = first_free_page; i < rstart; i++){
        if(coremap[i].is_clean && !coremap[i].is_busy){
            // tlb_shootdown_all_cpus(coremap[i].as,coremap[i].va);
            // coremap[i].is_busy = 1;
            return i;
        }
    }
    panic("Should not come here");
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
    spinlock_init(&swapout_spinlock);
    coremap = (struct coremap_entry*) PADDR_TO_KVADDR(first_address);
    free_address = first_address + total_num_pages * (sizeof(struct coremap_entry));
    first_free_page = free_address / PAGE_SIZE + 1;
    // Iterate all kernel entries
    for(index = 0; index < first_free_page ; index ++ ){
        set_busy(&coremap[index],1);
        set_cmap_fixed(&coremap[index], 1, NULL, PADDR_TO_KVADDR(index * PAGE_SIZE));
    }
    for(index = first_free_page ; index < total_num_pages; index ++ ){
        set_busy(&coremap[index],0);
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
    paddr_t p=0;
    KASSERT(npages>0);
    // KASSERT(!lock_do_i_hold(page_lock));
    if(npages>1){

        spinlock_acquire(&coremap_spinlock);
        
        p = find_continuous_block(false, npages);

        if(p == 0 && is_swapping){
            p = find_continuous_block(true, npages);
            if(p == 0){
                panic("Can't allocate continuous pages even with swapping enabled");
            }
        }

        
        bzero((void *)PADDR_TO_KVADDR(p), npages * PAGE_SIZE);
        spinlock_release(&coremap_spinlock);
    }
    else{
        //Single Page

        uint32_t i;
        spinlock_acquire(&coremap_spinlock);
        for( i = first_free_page; i < total_num_pages; i++){
            if(coremap[i].is_free && !coremap[i].is_busy){
                set_cmap_fixed(&coremap[i], 1, NULL, PADDR_TO_KVADDR(i * PAGE_SIZE));
                p = i * PAGE_SIZE;
                bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
                break;
            }
        }

        if(i>=total_num_pages && is_swapping){
            
            i = get_victim_index();
            if(i == 0){
                spinlock_release(&coremap_spinlock);
                return 0;
            }
            KASSERT(coremap[i].is_busy == 0);

            KASSERT(coremap[i].is_dirty || coremap[i].is_clean);
            set_busy(&coremap[i],1);
            swapout_page(i);
            // Update the new owner info
            set_cmap_fixed(&coremap[i], 1, NULL, PADDR_TO_KVADDR(i * PAGE_SIZE));
            p = i * PAGE_SIZE;
            bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
            set_busy(&coremap[i],0);
        }
        spinlock_release(&coremap_spinlock);
    }
    if(p==0 && !is_swapping){
        return 0;
    }

    return PADDR_TO_KVADDR(p);
};


paddr_t page_alloc(struct addrspace *as, vaddr_t va){
    // KASSERT(lock_do_i_hold(page_lock));
    KASSERT(va != 0);
    va &= PAGE_FRAME;
    uint32_t i;
    paddr_t p = 0;
    spinlock_acquire(&coremap_spinlock);
    for( i = first_free_page; i < total_num_pages; i++){
        if(coremap[i].is_free && !coremap[i].is_busy){
            p = i * PAGE_SIZE;
            set_cmap_dirty(&coremap[i], 1, as, va);
            bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
            break;
        }
    }

    // Check if we don't have ane free page left
    if(i>=total_num_pages && is_swapping){
        i = get_victim_index();
        if(i == 0){
            spinlock_release(&coremap_spinlock);
            return 0;
        }
        KASSERT(coremap[i].is_busy == 0);
        set_busy(&coremap[i],1);
        swapout_page(i);
        p = i * PAGE_SIZE;
        set_cmap_dirty(&coremap[i], 1, as, va);
        set_busy(&coremap[i],0);
        bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
    }
    spinlock_release(&coremap_spinlock);
    // lock_release(page_lock);
    return p;

};


void
free_pages(paddr_t physical_page_addr, vaddr_t v_addr){
    KASSERT( physical_page_addr % PAGE_SIZE == 0);
    // Refer PADDR_TO_KVADDR
    v_addr &= PAGE_FRAME;
    uint32_t cmap_index = physical_page_addr / PAGE_SIZE;
    uint32_t last_index;
        
    spinlock_acquire(&coremap_spinlock);
    // Get the size of the chunk
    size_t chunk_size = coremap[cmap_index].chunk_size;
    // Check if we are freeing a valid chunk
    KASSERT(chunk_size != 0);
    KASSERT(coremap[cmap_index].is_busy==0);  
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
    // uint32_t last_index;
        
    spinlock_acquire(&coremap_spinlock);
    // Get the size of the chunk
    uint32_t cmap_index = physical_page_addr / PAGE_SIZE ;
    size_t chunk_size = coremap[cmap_index].chunk_size;
    KASSERT(chunk_size != 0);
    KASSERT(coremap[cmap_index].is_busy==0);
    set_cmap_free(&coremap[cmap_index], 0, NULL, 0);    
    spinlock_release(&coremap_spinlock);
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
    uint32_t coremap_index;

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
                new_paddr &=PAGE_FRAME;
                pte = add_pte(as, faultaddress, new_paddr,permission);
                if(pte == NULL){
                    lock_release(page_lock);
                    
                    return ENOMEM;
                }
                KASSERT(pte->state != IN_DISK);
            }
            else{
                // Check if user has proper permission
                if(!has_permission(faulttype,pte)){
                    lock_release(page_lock);
                    return EFAULT;
                
                }
            }
            // lock_acquire(pte->pte_lock);
            if(is_swapping&&pte->state == IN_DISK){
                KASSERT(pte->physical_page_number == 0);
                swapin_page(as, faultaddress, pte);
                set_busy(&coremap[pte->physical_page_number/PAGE_SIZE],1);
                KASSERT(pte->state != IN_DISK);
                KASSERT(pte->physical_page_number != 0);
                KASSERT((pte->physical_page_number & PAGE_FRAME) == pte->physical_page_number);
                spinlock_acquire(&coremap_spinlock);
                
                if((permission & PF_R )== PF_R)
                {
                    set_cmap_clean(&coremap[pte->physical_page_number/PAGE_SIZE],1,as,faultaddress);    
                }
                else{
                    set_cmap_dirty(&coremap[pte->physical_page_number/PAGE_SIZE],1,as,faultaddress);   
                }
                spinlock_release(&coremap_spinlock);
                set_busy(&coremap[pte->physical_page_number/PAGE_SIZE],0);

            }
            if(is_swapping && (permission & PF_W)==PF_W){
                set_busy(&coremap[pte->physical_page_number/PAGE_SIZE],1);
                set_cmap_dirty(&coremap[pte->physical_page_number/PAGE_SIZE],1,as,faultaddress);   
                set_busy(&coremap[pte->physical_page_number/PAGE_SIZE],0);
            }
            
            spinlock_acquire(&coremap_spinlock);
            coremap_index = pte->physical_page_number / PAGE_SIZE;
            coremap[coremap_index].cmap_cpu = curcpu;
            spinlock_release(&coremap_spinlock);


            spinlock_acquire(&tlb_spinlock);
            // Add entry to tlb
            spl = splhigh();
            tlb_index = tlb_probe(faultaddress,0);
            if(tlb_index >= 0){
                // TLB fault
                splx(spl);
                panic("Duplicate tlb entry");
            }
            KASSERT(pte->physical_page_number!=0);

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

             spinlock_acquire(&coremap_spinlock);
            coremap_index = pte->physical_page_number / PAGE_SIZE;
            coremap[coremap_index].cmap_cpu = curcpu;
            spinlock_release(&coremap_spinlock);
            spinlock_acquire(&tlb_spinlock);
            spl = splhigh();
            tlb_index = tlb_probe(faultaddress,0);
            KASSERT(tlb_index>=0);
            uint32_t entry_hi, entry_lo;
            tlb_read(&entry_hi, &entry_lo, tlb_index);
            entry_lo |= TLBLO_DIRTY ;
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