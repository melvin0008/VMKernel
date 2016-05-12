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
#include <wchan.h>
#include <machine/tlb.h>
#include <swap_table_entry.h>


static struct spinlock coremap_spinlock;
static struct spinlock tlb_spinlock;
static uint32_t total_num_pages;
static uint32_t first_free_page;


static void swapout_page(struct page_table_entry *pte, int cmap_index);
static void swapin_page(paddr_t paddr, struct page_table_entry *pte);
static void invalidate_tlb(vaddr_t v_addr);


/*
CoreMap functions Start
*/

static void set_cmap_entry(struct coremap_entry *cmap, int state , size_t chunk_size, struct addrspace *as, vaddr_t va){
    cmap->state = state;
    cmap->chunk_size = chunk_size;
    cmap->as = as;
    cmap->va = va;
}

static void set_cmap_fixed(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,FIXED,chunk_size,as,va);
    cmap->cpu = NULL;
    cmap->tlbid = -1;
}


static void set_cmap_free(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,FREE,chunk_size,as,va);
    cmap->busy = 0;
    cmap->cpu = NULL;
    cmap->tlbid = -1;
}

static void set_cmap_dirty(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,DIRTY,chunk_size,as,va);
    cmap->cpu = curcpu;  
}
static void set_cmap_clean(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,CLEAN,chunk_size,as,va);
    cmap->cpu = curcpu;
}

static void set_busy(struct coremap_entry *cmap, bool is_busy){
    // KASSERT(cmap->busy!= is_busy);
    cmap->busy = is_busy;
}

/*
CoreMap functions END
*/


void invalidate_tlb(vaddr_t v_addr){
    uint32_t elo, ehi;
    paddr_t pa;
    spinlock_acquire(&tlb_spinlock);
    // int spl = splhigh(); 
        int tlb_index = tlb_probe(v_addr & PAGE_FRAME,0);
        if(tlb_index >= 0){
            tlb_read(&ehi, &elo, tlb_index);
            if(elo){
                pa=elo & PAGE_FRAME;
                KASSERT(pa!=0);
                coremap[pa/PAGE_SIZE].tlbid = -1;
                coremap[pa/PAGE_SIZE].cpu = NULL;
            }
            tlb_write(TLBHI_INVALID(tlb_index), TLBLO_INVALID(), tlb_index);
        }
    // splx(spl);
    spinlock_release(&tlb_spinlock);
}

static uint32_t get_victim(){
    
    unsigned i;
    lock_acquire(clock_hand_lock);
    unsigned temp = clock_hand;
    for(i = temp; i < total_num_pages; i++){
        struct page_table_entry *pte = coremap[i].pte;
        if(pte)
        {   
             if(pte->clock_bit == 0 && (coremap[i].state == DIRTY || coremap[i].state == CLEAN ) && !coremap[i].busy && pte->busy == 0){
                if(i!=total_num_pages-1)
                {
                    clock_hand = i+1;
                }
                else{
                    clock_hand = first_free_page;
                }
                coremap[i].busy = 1;
                lock_release(clock_hand_lock);
                return i;
            }
            if(pte->clock_bit ==1 && (coremap[i].state == DIRTY || coremap[i].state == CLEAN ) && !coremap[i].busy && pte->busy == 0){
                pte->clock_bit = 0;
            }
        }
    }    
    for(i = first_free_page; i < total_num_pages; i++){
        struct page_table_entry *pte = coremap[i].pte;
        if(pte)
        {   
             if(pte->clock_bit == 0 && (coremap[i].state == DIRTY || coremap[i].state == CLEAN ) && !coremap[i].busy && pte->busy == 0){
                if(i!=total_num_pages-1)
                {
                    clock_hand = i+1;
                }
                else{
                    clock_hand = first_free_page;
                }
                coremap[i].busy = 1;
                lock_release(clock_hand_lock);
                return i;
            }
            if(pte->clock_bit ==1 && (coremap[i].state == DIRTY || coremap[i].state == CLEAN ) && !coremap[i].busy && pte->busy == 0){
                pte->clock_bit = 0;
            }
        }
    }    
    lock_release(clock_hand_lock);
    panic("Should have decided some index by now");
    return 0;
}

//Ref :
//http://jhshi.me/2012/04/24/os161-coremap/index.html
void 
init_coremap(){
    paddr_t last_address = ram_getsize(); 
    paddr_t first_address = ram_getfirstfree();
    last_address -= last_address%PAGE_SIZE; 
    uint32_t index;
    paddr_t free_address;
    total_num_pages = last_address / PAGE_SIZE;
    spinlock_init(&coremap_spinlock);
    spinlock_init(&tlb_spinlock);
    coremap = (struct coremap_entry*) PADDR_TO_KVADDR(first_address);
    free_address = first_address + total_num_pages * (sizeof(struct coremap_entry));
    first_free_page = free_address / PAGE_SIZE + 1;
    clock_hand = first_free_page;
    // Iterate all kernel entries
    for(index = 0; index < first_free_page ; index ++ ){
        set_busy(&coremap[index],1);
        set_cmap_fixed(&coremap[index],1,NULL, PADDR_TO_KVADDR(index * PAGE_SIZE) );
        coremap[index].cpu = NULL;
        coremap[index].pte = NULL;
        coremap[index].tlbid = -1 ;
    }
    for(index = first_free_page ; index < total_num_pages; index ++ ){
        set_busy(&coremap[index],0);
        set_cmap_free(&coremap[index],0,NULL,0);
        coremap[index].cpu = NULL;
        coremap[index].pte = NULL;
        coremap[index].tlbid = -1;
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
    paddr_t p = 0;
    KASSERT(npages>0);
        //Multiple Pages
    spinlock_acquire(&coremap_spinlock);
    uint32_t start_page = 0;
    uint32_t j;
    uint32_t i;
    for(i = first_free_page; i<total_num_pages; i++ ){
        bool found_section = false;
        start_page = i;
        for( j = start_page; j < start_page+npages && j <total_num_pages; j++ ){
            if(coremap[j].state == FREE){
                found_section = true;
            }
            else{
                found_section = false;
                break;
            }
        }
        if(found_section && j-start_page == npages){
            set_busy(&coremap[start_page],1);
            set_cmap_fixed(&coremap[start_page],npages,NULL,PADDR_TO_KVADDR(start_page*PAGE_SIZE));
            for (uint32_t i = start_page+1; i < npages+start_page; i++){
                set_busy(&coremap[i],1);
                coremap[i].pte = NULL;
                set_cmap_fixed(&coremap[i],0,NULL, PADDR_TO_KVADDR(i* PAGE_SIZE));
            }       
            p = start_page * PAGE_SIZE;
            bzero((void *)PADDR_TO_KVADDR(p), npages * PAGE_SIZE);
            break;
        }
    }
    spinlock_release(&coremap_spinlock);
    if(i>=total_num_pages){
        if(is_swapping){
            KASSERT(npages==1);
            //get_victim
            uint32_t victim;
            // struct coremap_entry cmap_entry;
            struct page_table_entry *pte;

            //swapout
            victim =  get_victim();
            spinlock_acquire(&coremap_spinlock);
            pte = coremap[victim].pte;
            //swapout
            lock_acquire(pte->lock);
            pte->busy = true;
            swapout_page(pte, victim);
            pte->busy = false;
            coremap[victim].pte = pte;
            lock_release(pte->lock);

            set_cmap_fixed(&coremap[victim],1,NULL, PADDR_TO_KVADDR(victim* PAGE_SIZE));
            p = victim * PAGE_SIZE;
            bzero((void *)PADDR_TO_KVADDR(p), npages * PAGE_SIZE);
            set_busy(&coremap[victim],1);
            spinlock_release(&coremap_spinlock);
        }
        else{
            return 0;  
        }
    }

    return PADDR_TO_KVADDR(p);
};


paddr_t page_alloc(struct addrspace *as, vaddr_t va, struct page_table_entry *pte_send){
    uint32_t i;
    paddr_t p;
    // KASSERT(lock_do_i_hold(page_lock));
    spinlock_acquire(&coremap_spinlock);
    for( i = first_free_page; i < total_num_pages; i++){
        if(coremap[i].state == FREE){
            set_cmap_dirty(&coremap[i],1,as,va);
            coremap[i].cpu = NULL;
            coremap[i].tlbid = -1;
            coremap[i].pte = pte_send;
            p = i * PAGE_SIZE;
            bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
            break;
        }
    }
    spinlock_release(&coremap_spinlock);
    if(i>=total_num_pages){
        if(is_swapping){
            // get_victim
            uint32_t victim;
            // struct coremap_entry cmap_entry;
            struct page_table_entry *pte;
            

            //swapout
            victim =  get_victim();
            spinlock_acquire(&coremap_spinlock);
            pte = coremap[victim].pte;
            lock_acquire(pte->lock);
            pte->busy = true; 
            // spinlock_acquire(&coremap_spinlock);
            swapout_page(pte, victim);
            pte->busy = false; 
            coremap[victim].pte = pte_send;
            lock_release(pte->lock); 
            set_cmap_dirty(&coremap[victim],1,as, va);
            p = victim * PAGE_SIZE;
            bzero((void *)PADDR_TO_KVADDR(p), 1 * PAGE_SIZE);
            set_busy(&coremap[victim],0);
            spinlock_release(&coremap_spinlock);
        }
        else{
            return 0;
        }
    }
    return p;
};



void
free_kpages(vaddr_t v_addr){
    paddr_t paddr = v_addr -MIPS_KSEG0; 
    KASSERT( paddr % PAGE_SIZE == 0);
    uint32_t cmap_index = paddr / PAGE_SIZE;
    uint32_t last_index;
        
    spinlock_acquire(&coremap_spinlock);
    // Get the size of the chunk
    size_t chunk_size = coremap[cmap_index].chunk_size;
    // Check if we are freeing a valid chunk
    KASSERT(chunk_size != 0);
    KASSERT(coremap[cmap_index].busy==1);
    last_index = (cmap_index+chunk_size);
    for(; cmap_index <last_index ; cmap_index ++){
        set_busy(&coremap[cmap_index],0);
        set_cmap_free(&coremap[cmap_index],0,NULL,0);
        coremap[cmap_index].pte = NULL;
    }
    spinlock_release(&coremap_spinlock);
    invalidate_tlb(v_addr);

};


void page_free(paddr_t physical_page_addr, vaddr_t v_addr){

    KASSERT( physical_page_addr % PAGE_SIZE == 0);
    uint32_t cmap_index = physical_page_addr / PAGE_SIZE;   
    spinlock_acquire(&coremap_spinlock);
    // Get the size of the chunk
    size_t chunk_size = coremap[cmap_index].chunk_size;
    // Check if we are freeing a valid chunk
    KASSERT(chunk_size != 0);
    coremap[cmap_index].pte = NULL;

    KASSERT(coremap[cmap_index].busy==0);
    set_cmap_free(&coremap[cmap_index],0,NULL,0);
    spinlock_release(&coremap_spinlock);
    invalidate_tlb(v_addr);

};

void swapout_page(struct page_table_entry *pte, int cmap_index){
    KASSERT(pte->busy == true);

    KASSERT(pte != NULL);
    KASSERT(lock_do_i_hold(pte->lock));
    // Shootdown tlb entry
    // lock_acquire(pte->lock);
    struct coremap_entry cmap_entry = coremap[cmap_index];
    if(coremap[cmap_index].tlbid>=0){
        if(cmap_entry.cpu == curcpu){
            invalidate_tlb(cmap_entry.va);
        }
        else{
            KASSERT(cmap_entry.cpu!=NULL);
            struct tlbshootdown tl;
            tl.cmap_index = cmap_index;
            tl.tlbid = coremap[cmap_index].tlbid;

            ipi_tlbshootdown(coremap[cmap_index].cpu,(const struct tlbshootdown *) &tl);
            // while(coremap[cmap_index].tlbid!=-1){
                wchan_sleep(tlb_wchan,&coremap_spinlock);
                //This is like a JS callback
            // }
            // KASSERT(coremap[cmap_index].tlbid==-1);
        }
    }
    KASSERT(coremap[cmap_index].busy==1);
    coremap[cmap_index].cpu = NULL;
    coremap[cmap_index].tlbid = -1;
    KASSERT(pte->physical_page_number != 0);
    pte->clock_bit = 1;
    
    coremap[cmap_index].pte = pte;
    KASSERT((unsigned)(cmap_index * PAGE_SIZE) == pte->physical_page_number);
    // Copy contents from mem to disk
    if(coremap[cmap_index].state == DIRTY){
        spinlock_release(&coremap_spinlock);
        memory_to_swapdisk(pte->physical_page_number,pte->disk_position);
        spinlock_acquire(&coremap_spinlock);
    }
    pte->physical_page_number = 0;
    pte->state = IN_DISK;
    // lock_release(pte->lock);
    set_cmap_free(&coremap[cmap_index],1,NULL,0);
    bzero((void*) PADDR_TO_KVADDR(cmap_index*PAGE_SIZE),PAGE_SIZE);    
}

void swapin_page(paddr_t paddr, struct page_table_entry *pte){
    
    // Allocate a physical page
    // Find the swap slot using ste and
    // Copy stuff from disk to mem
    // lock_acquire(pte->lock);

    uint32_t cmap_index = paddr/ PAGE_SIZE ;
    KASSERT(coremap[cmap_index].state!= FIXED);
    set_busy(&coremap[cmap_index],1);
    KASSERT(pte->busy == true);
    swapdisk_to_memory(pte->disk_position, paddr);
    // Update the pte to indicate that the page is now in memory
    pte->physical_page_number = paddr;
    pte->state = IN_MEM;
    set_busy(&coremap[cmap_index],0);
    // lock_release(pte->lock);
}


unsigned int
coremap_used_bytes(void){
    unsigned int total_used_entries = 0, index;
    
    // TODO check if active waiting is costly
    spinlock_acquire(&coremap_spinlock);
    for(index = first_free_page ; index < total_num_pages; index += 1){
        if(!coremap[index].state == FREE){
            total_used_entries++;
        }
    }    
    spinlock_release(&coremap_spinlock);
    return total_used_entries * PAGE_SIZE ;
};

void 
vm_tlbshootdown_all(void){
    int i;

    /* Disable interrupts on this CPU while frobbing the TLB. */
    spinlock_acquire(&coremap_spinlock);
    // spl = splhigh();

    for (i=0; i<NUM_TLB; i++) {
        tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
    }
    wchan_wakeall(tlb_wchan,&coremap_spinlock);

    // splx(spl);
    spinlock_release(&coremap_spinlock);
        
};

void
vm_tlbshootdown(const struct tlbshootdown *tlb){
    uint32_t entry_lo, entry_hi;
    paddr_t paddr;
    spinlock_acquire(&coremap_spinlock);
    int index = tlb->cmap_index;
    if(coremap[index].cpu==curcpu && coremap[index].tlbid==tlb->tlbid ){
        tlb_read(&entry_hi, &entry_lo, tlb->tlbid);
        if(entry_lo){
            paddr=entry_lo & PAGE_FRAME;
            KASSERT(paddr!=0);
            coremap[paddr/PAGE_SIZE].tlbid = -1;
            coremap[paddr/PAGE_SIZE].cpu = NULL;
        }
        tlb_write(TLBHI_INVALID(tlb->tlbid), TLBLO_INVALID(), tlb->tlbid);
        coremap[index].tlbid=-1;
    }
    wchan_wakeall(tlb_wchan,&coremap_spinlock);
    spinlock_release(&coremap_spinlock);

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
        if((pte->permission & PF_R)==PF_R){
            return true;
        }
    }
    else if (faulttype == VM_FAULT_WRITE || faulttype == VM_FAULT_READONLY){
     if((pte->permission & PF_W) == PF_W){
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
    int permission, tlb_index;
    if(addr_region==NULL && !is_stack_or_heap){       
            return EFAULT;
    }
    if(addr_region == NULL){
        permission = PF_R | PF_W;
    }
    else{
        permission = addr_region->permission;
    }
    pte = search_pte(as, faultaddress);
    switch(faulttype){
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
            // 
            if(pte == NULL){
                // Allocate a new page (handled in add_pte)
                pte = add_pte(as, faultaddress, 0, permission);
                lock_acquire(pte->lock);
                pte->busy = true;
                pte->clock_bit = 1;
                paddr_t new_paddr = page_alloc(as,faultaddress,pte);
                pte->physical_page_number = new_paddr;
                coremap[pte->physical_page_number/PAGE_SIZE].pte = pte;
                
                if(pte == NULL){
                    pte->busy = false;
                    lock_release(pte->lock);
                    return ENOMEM;
                }
            }
            else{
                lock_acquire(pte->lock);
                pte->busy = true;
                // Check if user has proper permission
                if(!has_permission(faulttype,pte)){
                    pte->busy = false;
                    lock_release(pte->lock);
                    
                    return EFAULT;
                }
            }

            if(pte->state == IN_DISK){
                //pagealloc
                KASSERT(pte->physical_page_number==0);
                paddr_t new_paddr = page_alloc(as,faultaddress,pte);
                //swapin
                swapin_page(new_paddr,pte);
                KASSERT(pte->physical_page_number!=0); 
                KASSERT(pte->virtual_page_number==faultaddress); 
                
                // set clean
                if((permission & PF_R )== PF_R)
                {
                    set_cmap_clean(&coremap[pte->physical_page_number/PAGE_SIZE],1,as,faultaddress);    
                    pte->clock_bit = 1;
                    
                    coremap[pte->physical_page_number/PAGE_SIZE].pte = pte;
                }

            }
            if(is_swapping && (permission & PF_W) == PF_W){
                //set dirty
                pte->clock_bit = 1;
                set_busy(&coremap[pte->physical_page_number/PAGE_SIZE],1);
                set_cmap_dirty(&coremap[pte->physical_page_number/PAGE_SIZE],1,as,faultaddress);   
                coremap[pte->physical_page_number/PAGE_SIZE].pte = pte;
                set_busy(&coremap[pte->physical_page_number/PAGE_SIZE],0);
            }
            pte->clock_bit = 1;

            // Add entry to tlb
            
            spinlock_acquire(&coremap_spinlock);
            tlb_index = tlb_probe(faultaddress,0);
            KASSERT(tlb_index==-1);
            int random_entry_lo;
            random_entry_lo = pte->physical_page_number;
            random_entry_lo = random_entry_lo | ((pte->permission & PF_W) | 1) << 9;
            tlb_random((uint32_t) faultaddress, random_entry_lo);
            tlb_index = tlb_probe(faultaddress,0);
            KASSERT(tlb_index>=0);
            coremap[pte->physical_page_number/PAGE_SIZE].tlbid = tlb_index;
            coremap[pte->physical_page_number/PAGE_SIZE].cpu = curcpu;
            pte->busy = false;
            coremap[pte->physical_page_number/PAGE_SIZE].pte = pte;

            // set_busy(&coremap[pte->physical_page_number/PAGE_SIZE],0);
            spinlock_release(&coremap_spinlock);
            lock_release(pte->lock);
            // Now we know that there is an entry in the TLB
            break;
        case VM_FAULT_READONLY:
            KASSERT(pte!=NULL);
            lock_acquire(pte->lock);
            pte->busy = true;
            if(!has_permission(faulttype,pte)){
                    pte->busy = false;
                    lock_release(pte->lock);
                    return EFAULT;
                }
            pte->clock_bit = 1;
            spinlock_acquire(&coremap_spinlock  );
            tlb_index = tlb_probe(faultaddress,0);
            KASSERT(tlb_index>=0);
            uint32_t entry_hi, entry_lo;
            tlb_read(&entry_hi, &entry_lo, tlb_index);
            entry_lo |= TLBLO_DIRTY;
            tlb_write(entry_hi, entry_lo, tlb_index);
            coremap[pte->physical_page_number/PAGE_SIZE].tlbid = tlb_index;
            coremap[pte->physical_page_number/PAGE_SIZE].cpu = curcpu;
            pte->busy = false;
            coremap[pte->physical_page_number/PAGE_SIZE].pte = pte;
            spinlock_release(&coremap_spinlock);
            lock_release(pte->lock);
        break;
        default:
        return EFAULT;
    }
    return 0;
};