
#include <types.h>

struct swap_table_entry{
    vaddr_t va;
    struct addrspace *as;
    uint32_t swap_index;
};

struct swap_table_entry *create_ste(struct addrspace *as, vaddr_t va, uint32_t swap_index);
struct swap_table_entry *get_ste(struct addrspace *as, vaddr_t va);
void destroy_ste(struct swap_table_entry *ste);
