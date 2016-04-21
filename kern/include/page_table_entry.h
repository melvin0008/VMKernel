
// Assignment 3.2 ---> Virtual address Spaces stuff

#include <vm.h>

struct page_table_entry{
    vaddr_t virtual_page_number;
    paddr_t physical_page_number;
    int permission:3;
    bool state:1;
    bool valid:1;
    bool referenced:1;
    struct page_table_entry* next;
};

struct page_table_entry *create_page_table_entry(vaddr_t vpn, paddr_t ppn, int permission);
void destroy_page_table_entry(struct page_table_entry*);
struct page_table_entry *copy_pt(struct page_table_entry *,int32_t *);
struct page_table_entry *add_pte(struct addrspace *as, vaddr_t new_vaddr, int permission);
struct page_table_entry *search_pte(struct addrspace *as, vaddr_t va);
bool remove_pte_for(struct addrspace *as, vaddr_t va);
void destroy_pte_for(struct addrspace *as);