#include "paging.h"
#include "pmm.h"

#define KERNEL_VIRTUAL_BASE 0xC0000000

// The boot page directory is set up in boot.s
extern uint32_t boot_page_directory[1024];

// Pre-allocated page tables for user space mappings.
// Stored in kernel BSS so they are zeroed at boot and within a known
// virtual address range. We subtract KERNEL_VIRTUAL_BASE to get the
// physical address to put in the page directory.
static uint32_t user_page_tables[4][1024] __attribute__((aligned(4096)));
static int next_table = 0;

static uint32_t* alloc_page_table(void)
{
	if (next_table >= 4)
		return 0;
	return user_page_tables[next_table++];
}

void paging_init(void)
{
	// Paging is already set up in boot.s before kernel_main runs
	// This function is kept for future higher-level mapping helpers
}

// Map a physical page to a virtual address with given flags.
// Creates a page table if one doesn't exist for this virtual address yet.
void paging_map_page(uint32_t virt, uint32_t phys, uint32_t flags)
{
	uint32_t dir_index   = virt >> 22;
	uint32_t table_index = (virt >> 12) & 0x3FF;

	uint32_t* page_dir = boot_page_directory;

	if (!(page_dir[dir_index] & PAGE_PRESENT))
	{
		// No page table exists for this region — allocate one from our pool.
		// The pool lives in kernel BSS at a known virtual address, so we
		// subtract KERNEL_VIRTUAL_BASE to get the physical address for the
		// page directory entry.
		uint32_t* new_table = alloc_page_table();
		if (!new_table)
			return;

		uint32_t new_table_phys = (uint32_t)new_table - KERNEL_VIRTUAL_BASE;
		page_dir[dir_index] = new_table_phys | PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER;
	}

	// All page tables live in kernel BSS (virtual 0xC0xxxxxx).
	// The directory stores their physical addresses, so add KERNEL_VIRTUAL_BASE
	// to get the virtual address we can write through.
	uint32_t table_phys  = page_dir[dir_index] & ~0xFFF;
	uint32_t* page_table = (uint32_t*)(table_phys + KERNEL_VIRTUAL_BASE);

	// Install the mapping
	page_table[table_index] = (phys & ~0xFFF) | flags | PAGE_PRESENT;

	// Invalidate this address in the TLB so the CPU picks up the new mapping
	asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}