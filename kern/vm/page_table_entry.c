#include <page_table_entry.h>

// Create and init
struct page_table_entry*
create_page_table_entry(){

    struct page_table_entry *pte;
    pte = kmalloc(sizeof(*pte));
    if (pte == NULL) {
        return NULL;
    };
    pte->virtual_page_number = 0;
    pte->physical_page_number = 0;
    pte->permission = 0;
    pte->state = 0;
    pte->valid = 0;
    pte->referenced = 0;
    pte->next = NULL;
    return pte;
};

void destroy_page_table_entry(struct page_table_entry *pte){
    (void) pte;
    // Cleanup
};

struct page_table_entry * 
search_pte(struct addrspace *as, vaddr va){
    struct page_table_entry *pte_entry = as->pte_head;
    struct page_table_entry *next;
    vaddr vpn:20 = va & PAGE_FRAME;

    while(pte_entry != NULL){
        if(vpn == pte_entry->virtual_page_number){
            return pte_entry;
        }
        pte_entry = pte_entry->next;
    }   
    return NULL;
}