#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h> 
#include <synch.h>
#include <swap_table_entry.h>
#include <bitmap.h>

// static char kernel_buffer[PAGE_SIZE];
// Create and init


struct page_table_entry*
create_page_table_entry(vaddr_t vpn, paddr_t ppn, int permission){

    struct page_table_entry *pte = kmalloc(sizeof(*pte));
    if (pte == NULL) {
        return NULL;
    };
    pte->virtual_page_number = vpn;
    pte->physical_page_number = ppn;
    pte->permission = permission;
    pte->state = IN_MEM;
    if(is_swapping){
        pte->disk_position = get_clear_bit();
    }
    else{
        pte->disk_position = 0;
    }
    pte->lock = lock_create("pte-lock");
    pte->busy = false; 
    pte->next = NULL;
    return pte;
};

struct page_table_entry
*add_pte(struct addrspace *as, vaddr_t new_vaddr, paddr_t new_paddr,int permission){
    struct page_table_entry *new_pte = create_page_table_entry(new_vaddr,new_paddr,permission);
    if(new_pte == NULL){
        return NULL;
    }
    if(as->pte_head == NULL){
        as->pte_head = new_pte;
    }
    else{
        struct page_table_entry* pte_entry = as->pte_head;
        while(pte_entry->next != NULL){
            pte_entry = pte_entry->next;
        }
        pte_entry->next = new_pte;
        return pte_entry->next;
    }
    return new_pte;
}

struct
page_table_entry *copy_pt(struct addrspace *newas, struct page_table_entry *old_pte , int32_t *retval){    
    
    struct page_table_entry *temp;
    if(old_pte == NULL){
        *retval = 0;
        return NULL;
    }
    KASSERT(old_pte!=NULL);
    while(old_pte!=NULL){
        lock_acquire(old_pte->lock);
        old_pte->busy = true;
        old_pte->clock_bit = 1;
        struct page_table_entry *new_pte = add_pte(newas,old_pte->virtual_page_number,0,old_pte->permission);
        if(new_pte == NULL){
            *retval = ENOMEM;
            // lock_release(new_pte->lock);
            old_pte->busy = false;
            lock_release(old_pte->lock);
            return NULL;
        }
        lock_acquire(new_pte->lock);
        if(old_pte->state == IN_DISK){
            copy_swapdisk(old_pte->disk_position,new_pte->disk_position);
            new_pte->physical_page_number = 0;
        }
        else{
            // memmove((void *) kernel_buffer,(void *) PADDR_TO_KVADDR(old_pte->physical_page_number),PAGE_SIZE);
            new_pte->physical_page_number = page_alloc(newas,new_pte->virtual_page_number,new_pte);
            new_pte->clock_bit = 1;
            if(new_pte->physical_page_number == 0){
                *retval = ENOMEM;
                old_pte->busy = false;
                lock_release(new_pte->lock);
                lock_release(old_pte->lock);
                return NULL;
            }
            memmove((void *) PADDR_TO_KVADDR(new_pte->physical_page_number),(void *) PADDR_TO_KVADDR(old_pte->physical_page_number),PAGE_SIZE);
        }
        new_pte->state = old_pte->state;
        // lock_release(new_pte->lock);
        old_pte->busy = false;
        temp = old_pte->next;
        lock_release(new_pte->lock);
        lock_release(old_pte->lock);
        old_pte = temp;
    }
    return newas->pte_head;
}

void destroy_page_table_entry(struct page_table_entry *pte){
    (void) pte;
    // Cleanup
};

struct page_table_entry * 
search_pte(struct addrspace *as, vaddr_t va){
    struct page_table_entry *pte_entry = as->pte_head;
    vaddr_t vpn = va & PAGE_FRAME;

    while(pte_entry != NULL){
        if(vpn == pte_entry->virtual_page_number){
            return pte_entry;
        }
        pte_entry = pte_entry->next;
    }   
    return NULL;
}

bool
remove_pte_for(struct addrspace *as, vaddr_t va){
    struct page_table_entry *pte_entry = as->pte_head;
    struct page_table_entry *prev = pte_entry;
    vaddr_t vpn = va & PAGE_FRAME;
    if(pte_entry != NULL && pte_entry->next == NULL && vpn == pte_entry->virtual_page_number){
        lock_acquire(pte_entry->lock);
        pte_entry->busy = true;
        if(pte_entry->state == IN_MEM){
            page_free(pte_entry->physical_page_number, pte_entry->virtual_page_number);
        }
        else{
            KASSERT(bitmap_isset(swap_bitmap,as->pte_head->disk_position)!=0);
            bitmap_unmark(swap_bitmap,as->pte_head->disk_position);
        }
        pte_entry->busy = false;
        lock_release(pte_entry->lock);
        lock_destroy(pte_entry->lock);
        kfree(pte_entry);
        as->pte_head = NULL;
        return true;
    }

    while(pte_entry != NULL){
        if(vpn == pte_entry->virtual_page_number){
            break;
        }
        prev = pte_entry;
        pte_entry = pte_entry->next;
    }

    if(pte_entry != NULL){
        prev->next = pte_entry->next;
        pte_entry->next = NULL;
        lock_acquire(pte_entry->lock);
        pte_entry->busy = true;
        if(pte_entry->state == IN_MEM){
            page_free(pte_entry->physical_page_number, pte_entry->virtual_page_number);
        }
        else{
            KASSERT(bitmap_isset(swap_bitmap,as->pte_head->disk_position)!=0);
            bitmap_unmark(swap_bitmap,as->pte_head->disk_position);
        }
        pte_entry->busy = false;
        lock_release(pte_entry->lock);
        lock_destroy(pte_entry->lock);
        kfree(pte_entry);
        return true;
    }
    return false;
}

void
destroy_pte_for(struct addrspace *as){
    struct page_table_entry *next;
    struct page_table_entry *first = as->pte_head;
    // lock_acquire(page_lock);
    while(first != NULL){
        next = first;
        lock_acquire(next->lock);
        KASSERT(next->busy == false);
        next->busy = true;
        if(next->state==IN_MEM){
            page_free(next->physical_page_number, next->virtual_page_number);
        }
        else{
            KASSERT(bitmap_isset(swap_bitmap,next->disk_position)!=0);
            bitmap_unmark(swap_bitmap,next->disk_position);

        }
        next->busy = false;
        lock_release(next->lock);
        lock_destroy(next->lock);
        first = first->next;
        kfree(next);
    }
    as->pte_head = NULL;
    // lock_release(page_lock);
}