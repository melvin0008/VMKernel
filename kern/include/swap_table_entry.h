#define MAX_SWAP_TABLE_ENTIRES 8192 //32mb/4kb


struct vnode *swap_vn;
struct bitmap *swap_bitmap;
struct lock *swap_vnode_lock;
struct lock *page_lock;
struct lock *clock_hand_lock;

struct wchan *tlb_wchan;
bool is_swapping;


unsigned get_clear_bit(void);
void swap_disk_init(void);
void memory_to_swapdisk(paddr_t paddr,int disk_position);
void swapdisk_to_memory(int disk_position, paddr_t paddr);
void copy_swapdisk(int old_disk_position,int new_disk_position);