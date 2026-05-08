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
	// Step 1: Clear the page directory
	for (int i = 0; i < 1024; i++)
		page_directory[i] = 0;

	// Step 2: Identity map the first 4MB via page_table_0
	for (int i = 0; i < 1024; i++)
		page_table_0[i] = (i * 0x1000) | PAGE_PRESENT | PAGE_WRITEABLE;

	// Step 3: Install page_table_0 into the page directory
	page_directory[0] = (uint32_t)page_table_0 | PAGE_PRESENT | PAGE_WRITEABLE;

	// Step 4: load the page directory address into CR3
	asm volatile(
		"mov %0, %%cr3\n"
		"mov %%cr0, %%eax\n"
		"or $0x80000000, %%eax\n"
		"mov %%eax, %%cr0\n"
		: : "r"(page_directory) : "eax"
	);
}
