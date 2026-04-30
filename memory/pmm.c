#include "pmm.h"
#include "multiboot.h"

// one bit per 4KB page covering the first 4GB
// 4GB / 4KB = 104856 pages
// 104856 / 8 = 131072 bytes = 128kb for the bitmap
#define MAX_PAGES 1048576
#define BITMAP_SIZE (MAX_PAGES / 8)

static uint8_t bitmap[BITMAP_SIZE];
static uint32_t total_pages = 0;
static uint32_t free_pages = 0;

// Set a bit (mark page as used)
static void bitmap_set(uint32_t page)
{
	bitmap[page /8] |= (1 << (page % 8));
}

// clear a bit (mark page as free)
static void bitmap_clear(uint32_t page)
{
	bitmap[page / 8] &= ~(1 << (page % 8));
}

// Test bit (Check if bit is being used)
static int bitmap_test(uint32_t page)
{
	return bitmap[page / 8] & (1 << (page % 8));
}

void pmm_init(multiboot_info_t* mbi)
{
	// Everything starts marked as used (like ur mom GOTTEMMM)
	for (int i = 0; i < BITMAP_SIZE; i++)
		bitmap[i] = 0xFF;

	multiboot_mmap_t* mmap = (multiboot_mmap_t*) mbi->mmap_addr;
	multiboot_mmap_t* end = (multiboot_mmap_t*)(mbi->mmap_addr + mbi->mmap_length);

	while (mmap < end)
	{
		// Type 1 = useable RAM
		if (mmap->type == 1 && mmap->addr_high == 0)
		{
			uint32_t addr = mmap->addr_low;
			uint32_t len = mmap->len_low;

			// free each page in this region
			for (uint32_t i = 0; i < len / PAGE_SIZE; i++)
			{
				uint32_t page = (addr / PAGE_SIZE) + i;
				if (page < MAX_PAGES)
				{
					bitmap_clear(page);
					free_pages++;
					total_pages++;
				}
			}
		}
		// move to next entry
		mmap = (multiboot_mmap_t*)((uint32_t)mmap + mmap->size +4);
	}

	// remark bage 0 as used
	bitmap_set(0);

	// re mark kernel as used
	for (uint32_t page = (0x200000 / PAGE_SIZE); page < (0x400000 / PAGE_SIZE); page++)
	{
		if (!bitmap_test(page))
		{
			bitmap_set(page);
			free_pages--;
		}
	}
}


void* pmm_alloc_page(void)
{
	for (uint32_t i = 0; i < MAX_PAGES; i++)
	{
		if (!bitmap_test(i))
		{
			bitmap_set(i);
			free_pages--;
			return (void*)(i * PAGE_SIZE);
		}
	}
	return 0; // out of memorty
}

void pmm_free_page(void* addr)
{
	uint32_t page = (uint32_t)addr / PAGE_SIZE;
	bitmap_clear(page);
	free_pages++;
}

uint32_t pmm_free_pages (void)
{
	return free_pages;
}














