/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <current.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <elf.h>
#include <machine/tlb.h>
#include <spl.h>
#include <synch.h>
#include <swap_table_entry.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

/// Reference : http://jhshi.me/2012/04/24/os161-user-address-space/index.html

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL) {
		return NULL;
	}

	/*
	 * Initialize as needed.
	 */
	// Support a 4M stack space
	as->stack_end  = USERSTACK- (1024*PAGE_SIZE);
    as->heap_start = 0;
    as->heap_end = 0;
    as->region_head = NULL;
    as->pte_head =  NULL;
	return as;
}

int
as_copy(struct addrspace *old, struct addrspace **ret)
{
	// Recitation notes 
	// Usage example : kern/syscall/loadelf.c
	// 
	// Create an exact copy of the passed address space structure and also copy every page.
	// Feel free to implement Copy-On-Write (COW) page.
	struct addrspace *newas;

	newas = as_create();
	if (newas == NULL) {
		return ENOMEM;
	}

	/*
	 * Write this.
	 */
	int32_t retval_region = 0;
	int32_t retval_pte = 0;
	newas->stack_end  = old->stack_end;
    newas->heap_start = old->heap_start;
    newas->heap_end = old->heap_end;
    newas->region_head = copy_region(old->region_head,&retval_region);
    if(retval_region != 0){
    	return ENOMEM;
    }
    newas->pte_head = copy_pt(newas,old->pte_head,&retval_pte);
    if(retval_pte != 0){
    	return retval_pte;
    }
	*ret = newas;
	return 0;
}



void
as_destroy(struct addrspace *as)
{
	(void) as;
	/*
	 * Clean up as needed.
	 */
	// lock_acquire(page_lock);
    destroy_pte_for(as);
	destroy_regions_for(as);
	kfree(as);
	// lock_release(page_lock);

}


void
as_activate(void)
{
	// Recitation notes 
	// Usage example : kern/syscall/loadelf.c
	// 
	// Used when context switch happens. It invalidates all TLB entries and make
	// current process's address space the one currently seen by the processor.
	struct addrspace *as;

	as = proc_getas();
	if (as == NULL) {
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/*
	 * Write this.
	 */
	// Call tlb_shootdown 
	 vm_tlbshootdown_all();

}

void
as_deactivate(void)
{
	// Recitation notes 
	// Usage example : kern/syscall/loadelf.c
	// 
	// Unload current process's address space so it isn't seen by the processor.
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int
as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
		 int readable, int writeable, int executable)
{
	// Recitation notes 
	// Usage example : kern/syscall/loadelf.c
	// 
	// Setup a region of memory within the address space and store its
	// information, so you can check if the faultaddress is valid in vm_fault() or not.
	// Also make sure to adjust the heap start point.


	// // Adjust the heap
	// size_t heap_start = vaddr + memsize;
	// // Properly align the start
	// heap_start += (heap_start % PAGE_SIZE);	

	// Steal address translation from dumbvm
	/* Align the region. First, the base... */
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	int permission = readable | writeable | executable;

	int result = create_region(as,vaddr,memsize,permission,permission);
	if(result){
		return result;
	}
	
	as->heap_start = as->heap_end = vaddr + memsize;
	
	return 0;
}

int
as_prepare_load(struct addrspace *as)
{
	// Recitation notes
	// Usage example : kern/syscall/loadelf.c
	// 
	// Called before loading from an executable into the address space. 
	// It should setup page table entries for each region and 
	// make the pages writable despite of its real permission.
	/*
	 * Write this.
	 */

	struct addrspace_region *addr_region = as->region_head;
	while(addr_region != NULL){
		addr_region->permission = PF_W | PF_R;
		addr_region = addr_region->next;
	}
	return 0;
}

int
as_complete_load(struct addrspace *as)
{
	// Recitation notes
	// Usage example : kern/syscall/loadelf.c
	// 
	// Called when loading from an executable is complete. 
	// It should restore pages permission to what it was before calling as_prepare_load() 
	// and invalidate TLB entries if needed.
	/*
	 * Write this.
	 */

	struct addrspace_region *addr_region = as->region_head;
	while(addr_region != NULL){
		addr_region->permission = addr_region->orig_permission;
		addr_region = addr_region->next;
	}
	return 0;
}

int
as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	// Recitation notes
	// Usage example : kern/syscall/loadelf.c
	// 
	// Setup the stack region in the address space and 
	// return the initial stack pointer for the new process.
	/*
	 * Write this.
	 */

	(void)as;

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

