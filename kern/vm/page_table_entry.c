#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h> 

// static char kernel_buffer[PAGE_SIZE];
// Create and init


struct page_table_entry*
create_page_table_entry(vaddr_t vpn, paddr_t ppn, int permission){

    struct page_table_entry *pte = kmalloc(sizeof(*pte));
    if (pte == NULL) {
        return NULL;
    };
    pte->virtual_page_number = vpn;
    pte->physical_page_number = ppn;
    pte->permission = permission;
    pte->state = IN_MEM;
    pte->busy = 0;
    pte->next = NULL;
    return pte;
};

struct page_table_entry
*add_pte(struct addrspace *as, vaddr_t new_vaddr, paddr_t new_paddr,int permission){
    struct page_table_entry *new_pte = create_page_table_entry(new_vaddr,new_paddr,permission);
    if(new_pte == NULL){
        return NULL;
    }
    if(as->pte_head == NULL){
        as->pte_head = new_pte;
    }
    else{
        struct page_table_entry* pte_entry = as->pte_head;
        while(pte_entry->next != NULL){
            pte_entry = pte_entry->next;
        }
        pte_entry->next = new_pte;
        return pte_entry->next;
    }
    return new_pte;
}

struct
page_table_entry *copy_pt(struct addrspace *newas, struct page_table_entry *old_pte , int32_t *retval){    
    
    if(old_pte == NULL){
        *retval = 0;
        return NULL;
    }
    KASSERT(old_pte!=NULL);
    while(old_pte!=NULL){
        struct page_table_entry *new_pte = add_pte(newas,old_pte->virtual_page_number,0,old_pte->permission);
        if(new_pte == NULL){
            *retval = ENOMEM;
            return NULL;
        }
        new_pte->physical_page_number = page_alloc(newas,new_pte->virtual_page_number);
        if(new_pte->physical_page_number == 0){
            *retval = ENOMEM;
            return NULL;
        }
        memmove((void *) PADDR_TO_KVADDR(new_pte->physical_page_number),(void *) PADDR_TO_KVADDR(old_pte->physical_page_number),PAGE_SIZE);
        new_pte->state = old_pte->state;
        old_pte = old_pte->next;
    }
    return newas->pte_head;
}

void destroy_page_table_entry(struct page_table_entry *pte){
    (void) pte;
    // Cleanup
};

struct page_table_entry * 
search_pte(struct addrspace *as, vaddr_t va){
    struct page_table_entry *pte_entry = as->pte_head;
    vaddr_t vpn = va & PAGE_FRAME;

    while(pte_entry != NULL){
        if(vpn == pte_entry->virtual_page_number){
            return pte_entry;
        }
        pte_entry = pte_entry->next;
    }   
    return NULL;
}

bool
remove_pte_for(struct addrspace *as, vaddr_t va){
    struct page_table_entry *pte_entry = as->pte_head;
    struct page_table_entry *prev = pte_entry;
    vaddr_t vpn = va & PAGE_FRAME;

    if(pte_entry != NULL && pte_entry->next == NULL && vpn == pte_entry->virtual_page_number){
        page_free(pte_entry->physical_page_number, pte_entry->virtual_page_number);
        kfree(pte_entry);
        as->pte_head = NULL;
    }

    while(pte_entry != NULL){
        if(vpn == pte_entry->virtual_page_number){
            break;
        }
        prev = pte_entry;
        pte_entry = pte_entry->next;
    }

    if(pte_entry != NULL){
        prev->next = pte_entry->next;
        pte_entry->next = NULL;
        page_free(pte_entry->physical_page_number, pte_entry->virtual_page_number);
        kfree(pte_entry);
        return true;
    }

    return false;
}

void
destroy_pte_for(struct addrspace *as){
    struct page_table_entry *next;
    while(as->pte_head != NULL){
        next = as->pte_head->next;
        as->pte_head->next = NULL;
        page_free(as->pte_head->physical_page_number, as->pte_head->virtual_page_number);
        kfree(as->pte_head);
        as->pte_head = next;
    }
}