
// Assignment 3.2 ---> Virtual address Spaces stuff

#include <vm.h>
#define IN_DISK 0
#define IN_MEM 1

struct page_table_entry{
    vaddr_t virtual_page_number;
    paddr_t physical_page_number;
    int permission:3;
    bool state:1;
    bool busy:1;
    unsigned disk_position;
    struct lock *lock;
    struct page_table_entry* next;

};

struct page_table_entry *create_page_table_entry(vaddr_t vpn, paddr_t ppn, int permission);
void destroy_page_table_entry(struct page_table_entry*);
struct page_table_entry *copy_pt(struct addrspace *newas, struct page_table_entry *old_pte , int32_t *retval);
struct page_table_entry *add_pte(struct addrspace *as, vaddr_t new_vaddr, paddr_t new_paddr,int permission);
struct page_table_entry *search_pte(struct addrspace *as, vaddr_t va);
bool remove_pte_for(struct addrspace *as, vaddr_t va);
void destroy_pte_for(struct addrspace *as);