#include <swap_table_entry.h>
#include <lib.h>

struct swap_table_entry *create_ste(struct addrspace *as, vaddr_t va){
    struct swap_table_entry *ste = kmalloc(sizeof(*ste));
    if (ste == NULL) {
        return NULL;
    };
    ste->as = as;
    ste->va = va;

    return ste;
}

struct swap_table_entry *get_ste(struct addrspace *as, vaddr_t va){
    (void)as;
    (void)va;
    //TODO: Explore bitmap.h to look for an elegant way to do this shit!
    // Search the swap table
    for (int i = 0 ; i < MAX_SWAP_TABLE_ENTIRES; i++ ){
        if(swap_table[i] == NULL){
            swap_table[i] = create_ste(as,va);
            //Do a uinit to allocate memory
            return swap_table[i];
        }
        if(swap_table[i]->as == as && swap_table[i]->va == va){
            return swap_table[i];
        }
    }

    // Allocate a new slot if full
    return NULL;
}

void destroy_ste(struct swap_table_entry *ste){
    kfree(ste);
}
