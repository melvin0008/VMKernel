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