#include "paging.h"
#include "pmm.h"

// The boot page directory is set up in boot.s
extern uint32_t boot_page_directory[1024];

// Pre-allocated page tables for user space mappings, managed as a
// free list so tables return to the pool when an address space is
// destroyed (otherwise ~10 exec cycles would exhaust it).
#define USER_TABLE_POOL 32
static uint32_t user_page_tables[USER_TABLE_POOL][1024] __attribute__((aligned(4096)));
static uint32_t* table_free_list[USER_TABLE_POOL];
static int free_table_count = 0;

static uint32_t* alloc_page_table(void)
{
	if (free_table_count == 0)
		return 0;

	uint32_t* table = table_free_list[--free_table_count];

	// Recycled tables hold stale entries — hand out zeroed memory
	for (int i = 0; i < 1024; i++)
		table[i] = 0;

	return table;
}

// Return a table to the pool. Ignores pointers that didn't come from
// the pool (e.g. kernel tables copied from the boot directory).
static void free_page_table(uint32_t* table)
{
	uint32_t addr  = (uint32_t)table;
	uint32_t start = (uint32_t)user_page_tables;
	uint32_t end   = start + sizeof(user_page_tables);

	if (addr < start || addr >= end)
		return;

	if (free_table_count < USER_TABLE_POOL)
		table_free_list[free_table_count++] = table;
}

// Make sure the boot directory has a page table covering virt.
// Used to pre-create kernel-half tables before any process address
// space is made, so the (shared) table is copied into all of them.
static void ensure_kernel_table(uint32_t virt)
{
	uint32_t dir_index = virt >> 22;

	if (boot_page_directory[dir_index] & PAGE_PRESENT)
		return;

	uint32_t* table = alloc_page_table();
	if (!table)
		return;

	uint32_t table_phys = (uint32_t)table - KERNEL_VIRTUAL_BASE;
	boot_page_directory[dir_index] = table_phys | PAGE_PRESENT | PAGE_WRITEABLE;
}

void paging_init(void)
{
	// Paging itself is already set up in boot.s before kernel_main
	// runs; here we just stock the user page table pool
	free_table_count = 0;
	for (int i = 0; i < USER_TABLE_POOL; i++)
		table_free_list[free_table_count++] = user_page_tables[i];

	// Pre-create the ELF scratch window's table so every process
	// address space (which copies kernel directory entries at creation)
	// shares it — the loader must work no matter which CR3 is active
	ensure_kernel_table(ELF_SCRATCH_VIRT);
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
		uint32_t* new_table = alloc_page_table();
		if (!new_table)
			return;

		uint32_t new_table_phys = (uint32_t)new_table - KERNEL_VIRTUAL_BASE;
		page_dir[dir_index] = new_table_phys | PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER;
	}

	// Ensure the directory entry has the user bit set if this is a user mapping
	if (flags & PAGE_USER)
		page_dir[dir_index] |= PAGE_USER;

	// All page tables live in kernel BSS (virtual 0xC0xxxxxx).
	uint32_t table_phys  = page_dir[dir_index] & ~0xFFF;
	uint32_t* page_table = (uint32_t*)(table_phys + KERNEL_VIRTUAL_BASE);

	// Install the mapping
	page_table[table_index] = (phys & ~0xFFF) | flags | PAGE_PRESENT;

	// Invalidate this address in the TLB so the CPU picks up the new mapping
	asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

// Remove a mapping from the current (boot) address space.
void paging_unmap_page(uint32_t virt)
{
	uint32_t dir_index   = virt >> 22;
	uint32_t table_index = (virt >> 12) & 0x3FF;

	uint32_t* page_dir = boot_page_directory;

	if (!(page_dir[dir_index] & PAGE_PRESENT))
		return;

	uint32_t table_phys  = page_dir[dir_index] & ~0xFFF;
	uint32_t* page_table = (uint32_t*)(table_phys + KERNEL_VIRTUAL_BASE);

	page_table[table_index] = 0;
	asm volatile("invlpg (%0)" : : "r"(virt) : "memory");
}

// Create a new page directory with kernel mappings copied in.
// Returns the VIRTUAL address of the new page directory,
// or 0 on failure.
uint32_t* paging_create_address_space(void)
{
    // Allocate a page for the new directory from our pool
    uint32_t* new_dir = alloc_page_table();
    if (!new_dir)
        return 0;

    // Zero it out — user space starts empty
    uint32_t i;
    for (i = 0; i < 1024; i++)
        new_dir[i] = 0;

    // Copy kernel mappings (entries 768-1023, covering 0xC0000000+)
    // from the boot page directory into the new one.
    for (i = 768; i < 1024; i++)
        new_dir[i] = boot_page_directory[i];

    return new_dir;
}

// Destroy an address space: free its physical pages and return its
// page tables (and the directory itself) to the pool.
// Does NOT free kernel mappings (entries 768+). those are shared.
void paging_destroy_address_space(uint32_t* page_directory)
{
    if (!page_directory)
        return;

    // Free user space page tables (entries 0-767)
    uint32_t i;
    for (i = 0; i < 768; i++)
    {
        if (page_directory[i] & PAGE_PRESENT)
        {
            // Free each mapped page in this table
            uint32_t* table = (uint32_t*)((page_directory[i] & ~0xFFF) + KERNEL_VIRTUAL_BASE);
            uint32_t j;
            for (j = 0; j < 1024; j++)
            {
                if (table[j] & PAGE_PRESENT)
                    pmm_free_page((void*)(table[j] & ~0xFFF));
            }

            free_page_table(table);
            page_directory[i] = 0;
        }
    }

    free_page_table(page_directory);
}

// Map a page in a specific address space (not necessarily the current one).
// Returns 0 on success, -1 if no page table could be allocated.
int paging_map_page_in(uint32_t* page_directory, uint32_t virt, uint32_t phys, uint32_t flags)
{
    uint32_t dir_index   = virt >> 22;
    uint32_t table_index = (virt >> 12) & 0x3FF;

    if (!(page_directory[dir_index] & PAGE_PRESENT))
    {
        uint32_t* new_table = alloc_page_table();
        if (!new_table)
            return -1;

        uint32_t new_table_phys = (uint32_t)new_table - KERNEL_VIRTUAL_BASE;
        page_directory[dir_index] = new_table_phys | PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER;
    }

    if (flags & PAGE_USER)
        page_directory[dir_index] |= PAGE_USER;

    uint32_t table_phys  = page_directory[dir_index] & ~0xFFF;
    uint32_t* page_table = (uint32_t*)(table_phys + KERNEL_VIRTUAL_BASE);

    page_table[table_index] = (phys & ~0xFFF) | flags | PAGE_PRESENT;
    return 0;
}

// Switch the CPU to use a different address space.
// Pass the VIRTUAL address of the page directory.
void paging_switch_address_space(uint32_t* page_directory)
{
    uint32_t phys = (uint32_t)page_directory - KERNEL_VIRTUAL_BASE;
    asm volatile("mov %0, %%cr3" : : "r"(phys) : "memory");
}

// Switch back to the kernel's own (boot) address space.
void paging_switch_to_kernel(void)
{
    paging_switch_address_space(boot_page_directory);
}