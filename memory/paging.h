#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT    (1 << 0)  // Entry is valid and mapped
#define PAGE_WRITEABLE  (1 << 1)  // Page is writeable (clear = read only)
#define PAGE_USER       (1 << 2)  // Accessible from user mode (ring 3)

// The kernel half starts here. The first 4MB of physical memory is
// mapped at this address in every address space (boot.s entry 768,
// copied into each process directory).
#define KERNEL_VIRTUAL_BASE 0xC0000000

// Kernel-side scratch window used by the ELF loader to copy segment
// data into freshly allocated physical pages. Its page table is
// pre-created in paging_init so the mapping is shared by (and visible
// in) every address space created afterwards.
#define ELF_SCRATCH_VIRT 0xC0400000


uint32_t* paging_create_address_space(void);
void paging_destroy_address_space(uint32_t* page_directory);
// Free only the user half (entries 0-767), keeping the directory object
void paging_clear_user_space(uint32_t* page_directory);
// Eager-copy the user half of src into dst (src must be the active space).
// Returns 0 on success, -1 on out-of-memory.
int paging_copy_address_space(uint32_t* src_dir, uint32_t* dst_dir);
// Returns 0 on success, -1 if no page table could be allocated
int paging_map_page_in(uint32_t* page_directory, uint32_t virt, uint32_t phys, uint32_t flags);
void paging_switch_address_space(uint32_t* page_directory);
void paging_switch_to_kernel(void);
void paging_init(void);
void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags);
void paging_unmap_page(uint32_t virt);

#endif