
#include <types.h>
#define MAX_SWAP_TABLE_ENTIRES 8000 //32mb/4kb

struct swap_table_entry{
    vaddr_t va;
    struct addrspace *as;
};

struct swap_table_entry *swap_table[MAX_SWAP_TABLE_ENTIRES];
struct swap_table_entry *create_ste(struct addrspace *as, vaddr_t va);
struct swap_table_entry *get_ste(struct addrspace *as, vaddr_t va);
void destroy_ste(struct swap_table_entry *ste);
