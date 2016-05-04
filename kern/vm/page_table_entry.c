#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <bitmap.h>
#include <swap_table_entry.h>
#include <synch.h>
// static char kernel_buffer[PAGE_SIZE];
// Create and init
struct page_table_entry*
create_page_table_entry(vaddr_t vpn, paddr_t ppn, int permission){

    struct page_table_entry *pte = kmalloc(sizeof(*pte));
    if (pte == NULL) {
        panic ("Out of memory");
        return NULL;
    };
    // pte->pte_lock);
    pte->virtual_page_number = vpn;
    pte->physical_page_number = ppn;
    pte->permission = permission;
    pte->state = IN_MEM;
    pte->disk_position = get_clear_bit();
    pte->next = NULL;
    pte->pte_lock = lock_create("pte-lk");
    return pte;
}

struct page_table_entry
*add_pte(struct addrspace *as, vaddr_t new_vaddr, paddr_t new_paddr,int permission){
    struct page_table_entry *new_pte = create_page_table_entry(new_vaddr,new_paddr,permission);
    if(as->pte_head == NULL){
        as->pte_head = new_pte;
    }
    else{
        new_pte->next = as->pte_head;
        as->pte_head = new_pte;
    }
    return new_pte;
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

struct
page_table_entry *copy_pt(struct addrspace *newas, struct page_table_entry *old_pte , int32_t *retval){    
    
    if(old_pte == NULL){
        *retval = 0;
        return NULL;
    }
    KASSERT(old_pte!=NULL);
    KASSERT(old_pte->physical_page_number!=0);

    // lock_acquire(page_lock);
    while(old_pte!=NULL){
        lock_acquire(old_pte->pte_lock);
        struct page_table_entry *new_pte = add_pte(newas,old_pte->virtual_page_number,0,old_pte->permission);
        // bzero((void *)kernel_buffer, PAGE_SIZE);
        // if(old_pte->state== IN_MEM){
        //     memmove((void *) kernel_buffer,(void *)PADDR_TO_KVADDR(old_pte->physical_page_number),PAGE_SIZE);
        // }
        new_pte->physical_page_number = page_alloc(newas,old_pte->virtual_page_number);
        if(old_pte->state == IN_DISK ){
            copy_swapdisk(old_pte->disk_position, new_pte->disk_position);
        }
        else{
            memmove((void *) PADDR_TO_KVADDR(new_pte->physical_page_number),(void *)PADDR_TO_KVADDR(old_pte->physical_page_number),PAGE_SIZE);
        }
        lock_release(old_pte->pte_lock);
        old_pte = old_pte->next;
    }
    return newas->pte_head;
}

bool
remove_pte_for(struct addrspace *as, vaddr_t va){
    struct page_table_entry *pte_entry = as->pte_head;
    struct page_table_entry *prev = pte_entry;
    vaddr_t vpn = va & PAGE_FRAME;

    if(pte_entry != NULL && pte_entry->next == NULL && vpn == pte_entry->virtual_page_number){

        lock_acquire(pte_entry->pte_lock);
        // if(pte_entry->state == IN_DISK && bitmap_isset(swap_bitmap, pte_entry->disk_position)){
        //     bitmap_unmark(swap_bitmap, pte_entry->disk_position);
        // }

        page_free(pte_entry->physical_page_number, pte_entry->virtual_page_number);
        lock_release(pte_entry->pte_lock);
        lock_destroy(pte_entry->pte_lock);
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
        lock_acquire(pte_entry->pte_lock);
        if(pte_entry->state == IN_DISK &&bitmap_isset(swap_bitmap, pte_entry->disk_position)){
            bitmap_unmark(swap_bitmap, pte_entry->disk_position);
        }
        lock_release(as->pte_head->pte_lock);
        lock_destroy(pte_entry->pte_lock);
        page_free(pte_entry->physical_page_number, pte_entry->virtual_page_number);
        kfree(pte_entry);
        return true;
    }

    return false;
}

void
destroy_pte_for(struct addrspace *as){
    struct page_table_entry *next;

    // lock_acquire(page_lock);
    while(as->pte_head != NULL){
        lock_acquire(as->pte_head->pte_lock);
        next = as->pte_head->next;
        as->pte_head->next = NULL;
        
        // if(as->pte_head->state == IN_DISK && bitmap_isset(swap_bitmap, as->pte_head->disk_position)){
            bitmap_unmark(swap_bitmap, as->pte_head->disk_position);
        // }
        if(as->pte_head->state == IN_MEM){
            page_free(as->pte_head->physical_page_number, as->pte_head->virtual_page_number);
        }
        lock_release(as->pte_head->pte_lock);
        lock_destroy(as->pte_head->pte_lock);
        kfree(as->pte_head);
        as->pte_head = next;
    }
     // lock_release(page_lock);

}