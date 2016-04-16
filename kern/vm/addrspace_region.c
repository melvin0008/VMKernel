#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>

struct
addrspace_region *copy_region(struct addrspace_region *old_region , int32_t *retval){
    if(old_region == NULL){
        *retval = 0;
        return NULL;
    }
    struct addrspace_region *new_region = kmalloc(sizeof(struct addrspace_region));
    if(new_region == NULL){
        *retval = ENOMEM;
        return NULL;
    }
    new_region->permission = old_region->permission;
    new_region->size = old_region->size;
    new_region->start = old_region->start;
    new_region->next = copy_region(old_region->next,retval);
    if(*retval!=0){
        return NULL;
    }
    *retval = 0;
    return new_region;
}

int
set_region_data(struct addrspace *as, vaddr_t vaddr, size_t memsize, int permission){
    struct addrspace_region *new_region = kmalloc(sizeof(struct addrspace_region));
    if(new_region == NULL){
        return ENOMEM;
    }
    new_region->permission = permission;
    new_region->size = memsize;
    new_region->start = vaddr;
    new_region->next = NULL;

    // Attach it to the end
    if(as->region_head == NULL){
        as->region_head = new_region;
    }
    else{
        struct addrspace_region *last = as->region_head;
        while(last->next != NULL){
            last = last->next;
        }
        last->next = new_region;
    }
    return 0;
}