#define MAX_SWAP_TABLE_ENTIRES 8000 //32mb/4kb


struct vnode *swap_vn;
struct bitmap *swap_bitmap;

unsigned get_clear_bit(void);
void swap_disk_init(void);
void memory_to_swapdisk(int cmap_index);
void swapdisk_to_memory(struct page_table_entry *pte, paddr_t paddr);
unsigned copy_swapdisk(unsigned old_disk_position);