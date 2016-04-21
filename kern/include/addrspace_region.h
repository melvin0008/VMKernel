
#include <vm.h>
#include <addrspace.h>

struct addrspace;

struct addrspace_region {
  int permission;
  int orig_permission;
  size_t size;
  vaddr_t start;
  struct addrspace_region *next;
};

struct addrspace_region *copy_region(struct addrspace_region *, int32_t *);

int create_region(struct addrspace *as, vaddr_t vaddr, size_t memsize, int permission, int orig_permission);

struct addrspace_region *get_region_for(struct addrspace *as, vaddr_t faultaddress);

void destroy_regions_for(struct addrspace *as);