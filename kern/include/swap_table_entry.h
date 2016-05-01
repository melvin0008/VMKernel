#define MAX_SWAP_TABLE_ENTIRES 8000 //32mb/4kb

struct swap_table_entry{
    vaddr_t va;
    struct addrspace *as;
};

struct vnode *swap_vn;
struct swap_table_entry *swap_table[MAX_SWAP_TABLE_ENTIRES];

struct swap_table_entry *create_ste(struct addrspace *as, vaddr_t va);
void add_ste(struct addrspace *as, vaddr_t va);
int get_ste_position(struct addrspace *as, vaddr_t va);
void destroy_ste(struct swap_table_entry *ste);
void swap_disk_init(void);
void memory_to_swapdisk(int cmap_index);
void swapdisk_to_memory(struct addrspace *as, vaddr_t va,paddr_t paddr);
