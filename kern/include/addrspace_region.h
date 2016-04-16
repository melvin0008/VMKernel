
#include <vm.h>
#include <addrspace.h>

struct addrspace;

struct addrspace_region {
  int permission;
  size_t size;
  vaddr_t start;
  struct addrspace_region *next;
};

struct
addrspace_region *copy_region(struct addrspace_region *, int32_t *);

int
set_region_data(struct addrspace *as, vaddr_t vaddr, size_t memsize, int permission);