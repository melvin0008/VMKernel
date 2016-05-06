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

static struct spinlock coremap_spinlock;
static struct spinlock tlb_spinlock;
static uint32_t total_num_pages;
static uint32_t first_free_page;


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
}


static void set_cmap_free(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,FREE,chunk_size,as,va);
    cmap->cpu = NULL;
}

static void set_cmap_dirty(struct coremap_entry *cmap , size_t chunk_size, struct addrspace *as, vaddr_t va){
    set_cmap_entry(cmap,DIRTY,chunk_size,as,va);
}

static void set_busy(struct coremap_entry *cmap, bool is_busy){
    KASSERT(cmap->busy!= is_busy);
    cmap->busy = is_busy;
}
// static void set_cmap_clean(struct coremap_entry *cmap , size_t chunk_size){
//     set_cmap_entry(&cmap,false,false,false,true,chunk_size);
// }


/*
CoreMap functions END
*/


static void invalidate_tlb(vaddr_t v_addr){

    spinlock_acquire(&tlb_spinlock);
    int spl = splhigh(); 
        int tlb_index = tlb_probe(v_addr & PAGE_FRAME,0);
        if(tlb_index >= 0){
            tlb_write(TLBHI_INVALID(tlb_index), TLBLO_INVALID(), tlb_index);
        }
    splx(spl);
    spinlock_release(&tlb_spinlock);
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
    // Iterate all kernel entries
    for(index = 0; index < first_free_page ; index ++ ){
        set_busy(&coremap[index],1);
        set_cmap_fixed(&coremap[index],1,NULL, PADDR_TO_KVADDR(index * PAGE_SIZE) );
    }
    for(index = first_free_page ; index < total_num_pages; index ++ ){
        set_cmap_free(&coremap[index],0,NULL,0);
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
            set_cmap_fixed(&coremap[start_page],npages,NULL,PADDR_TO_KVADDR(start_page*PAGE_SIZE));
            for (uint32_t i = start_page+1; i < npages+start_page; i++){
                set_cmap_fixed(&coremap[i],0,NULL, PADDR_TO_KVADDR(i* PAGE_SIZE));
            }       
            p = start_page * PAGE_SIZE;
            spinlock_release(&coremap_spinlock);
            bzero((void *)PADDR_TO_KVADDR(p), npages * PAGE_SIZE);
            break;
        }
    }
    if(i>=total_num_pages){
       spinlock_release(&coremap_spinlock);
       return 0;
    }
    
    return PADDR_TO_KVADDR(p);
};


paddr_t page_alloc(struct addrspace *as, vaddr_t va){
    uint32_t i;
    paddr_t p;
    spinlock_acquire(&coremap_spinlock);
    for( i = first_free_page; i < total_num_pages; i++){
        if(coremap[i].state == FREE){
            set_cmap_dirty(&coremap[i],1,as,va);
            p = i * PAGE_SIZE;
            bzero((void *)PADDR_TO_KVADDR(p), PAGE_SIZE);
            break;
        }
    }
    spinlock_release(&coremap_spinlock);
    if(i>=total_num_pages){
        // panic("No Page left");
        return 0;
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
    
    last_index = (cmap_index+chunk_size);
    for(; cmap_index <last_index ; cmap_index ++){
        set_cmap_free(&coremap[cmap_index],0,NULL,0);
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
    set_cmap_free(&coremap[cmap_index],0,NULL,0);
    spinlock_release(&coremap_spinlock);
    invalidate_tlb(v_addr);

};




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
    int i, spl;

    /* Disable interrupts on this CPU while frobbing the TLB. */
    spinlock_acquire(&tlb_spinlock);
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

    pte = search_pte(as, faultaddress);
    switch(faulttype){
        case VM_FAULT_READ:
        case VM_FAULT_WRITE:
            // 
            if(pte == NULL){
                // Allocate a new page (handled in add_pte)
                paddr_t new_paddr = page_alloc(as,faultaddress);
                pte = add_pte(as, faultaddress, new_paddr, permission);
                if(pte == NULL){
                    return ENOMEM;
                }
            }
            else{
                // Check if user has proper permission
                if(!has_permission(faulttype,pte)){
                    return EFAULT;
                }
            }
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
            spinlock_acquire(&coremap_spinlock);
            coremap[pte->physical_page_number/PAGE_SIZE].cpu = curcpu;
            spinlock_release(&coremap_spinlock);
            // Now we know that there is an entry in the TLB
            break;
        case VM_FAULT_READONLY:
            KASSERT(pte!=NULL);
            if(!has_permission(faulttype,pte)){
                    return EFAULT;
                }
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
            spinlock_acquire(&coremap_spinlock);
            coremap[pte->physical_page_number/PAGE_SIZE].cpu = curcpu;
            spinlock_release(&coremap_spinlock);
        break;
        default:
        return EFAULT;
    }
    return 0;
};