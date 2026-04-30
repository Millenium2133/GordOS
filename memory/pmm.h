#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>
#include  "multiboot.h"

#define PAGE_SIZE 4096

void pmm_init(multiboot_info_t* mbi);
void* pmm_alloc_page(void);
void pmm_free_page(void* addr);
uint32_t pmm_free_pages(void);

#endif
