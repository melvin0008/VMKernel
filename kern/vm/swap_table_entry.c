#include <swap_table_entry.h>
#include <lib.h>

struct swap_table_entry *create_ste(struct addrspace *as, vaddr_t va, uint32_t swap_index){
    struct swap_table_entry *ste = kmalloc(sizeof(*ste));
    if (ste == NULL) {
        return NULL;
    };
    ste->as = as;
    ste->va = va;
    ste->swap_index = swap_index;

    return ste;
}

struct swap_table_entry *get_ste(struct addrspace *as, vaddr_t va){
    (void)as;
    (void)va;

    // Search the swap table

    // Allocate a new slot if not found
    return NULL;
}

void destroy_ste(struct swap_table_entry *ste){
    kfree(ste);
}
