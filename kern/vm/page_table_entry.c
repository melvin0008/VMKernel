#include <page_table_entry.h>

// Create and init
struct page_table_entry*
create_page_table_entry(vaddr_t vpn, paddr_t ppn){

    struct page_table_entry *pte;
    pte = kmalloc(sizeof(*pte));
    if (pte == NULL) {
        return NULL;
    };
    pte->virtual_page_number = vpn;
    pte->physical_page_number = ppn;
    pte->permission = 0;
    pte->state = false;
    pte->valid = false;
    pte->referenced = false;
    pte->next = NULL;
    return pte;
};

struct page_table_entry*
add_pte(addrspace as, vaddr_t vpn, paddr_t ppn){

    if(as->pte_head == NULL){
        as->pte_head = create_page_table_entry(vpn,ppn);
        return as->pte_head;
    }
    else{
        struct page_table_entry* pte_entry = as->pte_head;
        while(pte_entry->next != NULL){
            pte_entry = pte_entry->next;
        }
        pte_entry->next = create_page_table_entry(vpn,ppn);
        return pte_entry->next;
    }
    
}

void destroy_page_table_entry(struct page_table_entry *pte){
    (void) pte;
    // Cleanup
};
