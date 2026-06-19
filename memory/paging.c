#include "paging.h"
#include "pmm.h"
#include "string.h"

// The boot page directory is set up in boot.s
extern uint32_t boot_page_directory[1024];

// Second scratch window for fork's page copies. It lives in the same
// 4MB region as ELF_SCRATCH_VIRT (0xC04xxxxx), so the page table that
// ensure_kernel_table created for the ELF scratch already covers it —
// no extra setup needed. fork and the ELF loader never run at the same
// time (the kernel is single-threaded with interrupts off during a
// syscall), so a separate address just keeps their uses from aliasing.
#define FORK_SCRATCH_VIRT  0xC0401000

// Third scratch window for building user stack frames (argc/argv). Same
// shared 4MB table, distinct page from the other two scratch windows.
#define STACK_SCRATCH_VIRT 0xC0402000

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

// Free the user half of an address space (entries 0-767): release every
// mapped physical page and return the page tables to the pool, leaving
// the directory object itself intact (kernel mappings 768+ are shared
// and untouched). Used by exec to reuse a directory in place.
void paging_clear_user_space(uint32_t* page_directory)
{
    if (!page_directory)
        return;

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
}

// Destroy an address space: free its user pages/tables (above) and then
// return the directory itself to the pool.
void paging_destroy_address_space(uint32_t* page_directory)
{
    if (!page_directory)
        return;

    paging_clear_user_space(page_directory);
    free_page_table(page_directory);
}

// Eagerly copy the user half (entries 0-767) of src_dir into dst_dir:
// for every present page, allocate a fresh physical page, copy the
// contents, and map it into dst with the same flags. No copy-on-write.
//
// Assumes src_dir is the CURRENTLY ACTIVE address space, so source
// pages can be read directly at their user virtual addresses. Returns
// 0 on success, -1 on out-of-memory (caller destroys the half-built
// child, which frees whatever was mapped so far).
int paging_copy_address_space(uint32_t* src_dir, uint32_t* dst_dir)
{
    if (!src_dir || !dst_dir)
        return -1;

    uint32_t d;
    for (d = 0; d < 768; d++)
    {
        if (!(src_dir[d] & PAGE_PRESENT))
            continue;

        uint32_t* src_table = (uint32_t*)((src_dir[d] & ~0xFFF) + KERNEL_VIRTUAL_BASE);

        uint32_t t;
        for (t = 0; t < 1024; t++)
        {
            if (!(src_table[t] & PAGE_PRESENT))
                continue;

            uint32_t flags = src_table[t] & 0xFFF;
            uint32_t vaddr = (d << 22) | (t << 12);

            void* dst_phys = pmm_alloc_page();
            if (!dst_phys)
                return -1;

            // Map the new page at the scratch window, copy the source
            // page (readable at its own vaddr under the active cr3),
            // then drop the scratch mapping.
            paging_map_page(FORK_SCRATCH_VIRT, (uint32_t)dst_phys, PAGE_PRESENT | PAGE_WRITEABLE);
            memcpy((void*)FORK_SCRATCH_VIRT, (void*)vaddr, 0x1000);
            paging_unmap_page(FORK_SCRATCH_VIRT);

            if (paging_map_page_in(dst_dir, vaddr, (uint32_t)dst_phys, flags) != 0)
            {
                pmm_free_page(dst_phys);
                return -1;
            }
        }
    }

    return 0;
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

#define STACK_MAX_ARGS 16
#define STACK_MAX_ARG_LEN 256

uint32_t paging_build_user_stack(void* stack_phys, uint32_t user_page_base,
                                  const char* cmdline)
{
    // Map the physical stack page into the kernel scratch window
    paging_map_page(STACK_SCRATCH_VIRT, (uint32_t)stack_phys,
                    PAGE_PRESENT | PAGE_WRITEABLE);
    uint8_t* page = (uint8_t*)STACK_SCRATCH_VIRT;

    memset(page, 0, 0x1000);

    // Tokenise cmdline
    char tokens[STACK_MAX_ARGS][STACK_MAX_ARG_LEN];
    int argc = 0;
    const char* p = cmdline ? cmdline : "";
    while (*p && argc < STACK_MAX_ARGS)
    {
        while (*p == ' ') p++;
        if (!*p) break;
        int j = 0;
        while (*p && *p != ' ' && j < STACK_MAX_ARG_LEN - 1)
            tokens[argc][j++] = *p++;
        tokens[argc][j] = '\0';
        argc++;
    }

    // Pack argument strings from the top of the page downward (highest
    // index first so argv[0] ends up lowest, but order doesn't matter
    // since we track each string's user-space virtual address).
    int top = 0x1000;
    uint32_t str_user_ptrs[STACK_MAX_ARGS];

    for (int i = argc - 1; i >= 0; i--)
    {
        int slen = 0;
        while (tokens[i][slen]) slen++;
        slen++;  // include NUL
        top -= slen;
        for (int k = 0; k < slen; k++)
            page[top + k] = tokens[i][k];
        str_user_ptrs[i] = user_page_base + (uint32_t)top;
    }

    // Align to 4 bytes
    top &= ~3;

    // argv pointer array: NULL sentinel first (growing downward)
    top -= 4;
    *(uint32_t*)(page + top) = 0;
    for (int i = argc - 1; i >= 0; i--)
    {
        top -= 4;
        *(uint32_t*)(page + top) = str_user_ptrs[i];
    }

    // argv_user_addr is the address of argv[0] in user space
    uint32_t argv_user_addr = user_page_base + (uint32_t)top;

    // Standard C i686 _start frame:
    //   [esp]    = fake return address (0)
    //   [esp+4]  = argc
    //   [esp+8]  = argv  (pointer to argv[0])
    //   [esp+12] = envp  (NULL)
    top -= 4; *(uint32_t*)(page + top) = 0;              // envp
    top -= 4; *(uint32_t*)(page + top) = argv_user_addr; // argv
    top -= 4; *(uint32_t*)(page + top) = (uint32_t)argc; // argc
    top -= 4; *(uint32_t*)(page + top) = 0;              // fake return addr

    paging_unmap_page(STACK_SCRATCH_VIRT);

    return user_page_base + (uint32_t)top;
}