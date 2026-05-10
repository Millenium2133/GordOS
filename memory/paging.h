#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT    (1 << 0)  // Entry is valid and mapped
#define PAGE_WRITEABLE  (1 << 1)  // Page is writeable (clear = read only)
#define PAGE_USER       (1 << 2)  // Accessible from user mode (ring 3)

void paging_init(void);
void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags);

#endif