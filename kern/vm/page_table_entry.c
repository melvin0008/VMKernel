#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h> 
#include <page_table_entry.h>

// Create and init
struct page_table_entry*
create_page_table_entry(vaddr_t vpn, paddr_t ppn, int permission){

    struct page_table_entry *pte = kmalloc(sizeof(struct page_table_entry));
    if (pte == NULL) {
        return NULL;
    };
    pte->virtual_page_number = vpn;
    pte->physical_page_number = ppn;
    pte->permission = permission;
    pte->state = false;
    pte->valid = false;
    pte->referenced = false;
    pte->next = NULL;
    return pte;
};

struct page_table_entry
*add_pte(struct addrspace *as, vaddr_t new_vaddr, int permission){

    paddr_t new_paddr = page_alloc();
    if(as->pte_head == NULL){
        as->pte_head = create_page_table_entry(new_vaddr, new_paddr, permission);
        return as->pte_head;
    }
    else{
        struct page_table_entry* pte_entry = as->pte_head;
        while(pte_entry->next != NULL){
            pte_entry = pte_entry->next;
        }
        pte_entry->next = create_page_table_entry(new_vaddr, new_paddr, permission);
        return pte_entry->next;
    }
    
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
page_table_entry *copy_pt(struct page_table_entry *old_pte , int32_t *retval){
    if(old_pte == NULL){
        *retval = 0;
        return NULL;
    }
    struct page_table_entry *new_pte = kmalloc(sizeof(struct page_table_entry));
    if(new_pte == NULL){
        *retval = ENOMEM;
        return NULL;
    }
    // TODO Use add_pte if possible !
    new_pte->virtual_page_number = old_pte->virtual_page_number;
    new_pte->physical_page_number = old_pte->physical_page_number;
    new_pte->permission = old_pte->permission;
    new_pte->state = old_pte->state;
    new_pte->referenced = old_pte->referenced;
    new_pte->next = copy_pt(old_pte->next,retval);
    if(*retval!=0){
        return NULL;
    }
    *retval = 0;
    return new_pte;
}