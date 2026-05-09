#include "paging.h"
#include "pmm.h"

// Page directory is 1024 entries, each pointing to a page table.
// Must be 4KB aligned so the low 12 bits are free for CR3 flags.
static uint32_t page_directory[1024] __attribute__((aligned(4096)));

// Identity map the first 4MB of memory (The firt 2 page tables)
// which covers the kernel at 2MB and gives a little breathing room.
static uint32_t page_table_0[1024] __attribute__((aligned(4096))); // 0MB - 4MB

void paging_init(void)
{
	// Paging is set up in boot.s before kerel.main runs (higher half kernel)
	// This file will host higher level mapping helpers later
}
