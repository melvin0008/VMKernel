#define MAX_SWAP_TABLE_ENTIRES 8000 //32mb/4kb


struct vnode *swap_vn;
struct bitmap *swap_bitmap;
struct lock *page_lock;
struct lock *copy_lock;

unsigned get_clear_bit(void);
void swap_disk_init(void);
void memory_to_swapdisk(int cmap_index, struct page_table_entry *pte);
void swapdisk_to_memory(struct page_table_entry *pte, paddr_t paddr);
void copy_swapdisk(unsigned old_disk_position, unsigned new_disk_position);