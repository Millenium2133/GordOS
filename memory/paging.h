#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT    (1 << 0)  // Entry is valid and mapped
#define PAGE_WRITEABLE  (1 << 1)  // Page is writeable (clear = read only)
#define PAGE_USER       (1 << 2)  // Accessible from user mode (ring 3)


uint32_t* paging_create_address_space(void);
void paging_destroy_address_space(uint32_t* page_directory);
void paging_map_page_in(uint32_t* page_directory, uint32_t virt, uint32_t phys, uint32_t flags);
void paging_switch_address_space(uint32_t* page_directory);
void paging_init(void);
void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

#endif