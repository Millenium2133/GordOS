#include "kmalloc.h"
#include "pmm.h"
#include <stdint.h>

// Header that sits in front of every allocation
typedef struct block_header
{
    size_t size;                // size of the data region (not including header)
    int free;                   // 1 = free, 0 = in use
    struct block_header* next;  // next block in the chain
} block_header_t;

#define HEADER_SIZE sizeof(block_header_t)

static block_header_t* heap_start = 0;

// Ask PMM for a new page and turn it into a block
static block_header_t* new_block(size_t size)
{
    // How many pages do we need?
    size_t pages_needed = (size + HEADER_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;

    block_header_t* block = 0;

    for (size_t i = 0; i < pages_needed; i++)
    {
        void* page = pmm_alloc_page();
        if (!page)
            return 0; // out of memory

        // First page becomes the block header
        if (i == 0)
            block = (block_header_t*)page;
    }

    block->size = (pages_needed * PAGE_SIZE) - HEADER_SIZE;
    block->free = 0;
    block->next = 0;
    return block;
}

void kmalloc_init(void)
{
    heap_start = 0;
}

void* kmalloc(size_t size)
{
    if (size == 0)
        return 0;

    // Align size to 4 bytes so allocations are always aligned
    size = (size + 3) & ~3;

    // Walk the chain looking for a free block that's big enough
    block_header_t* current = heap_start;
    block_header_t* previous = 0;

    while (current)
    {
        if (current->free && current->size >= size)
        {
            // Found a fit - split it if there's enough room left over
            // for another header plus at least 4 bytes of data
            if (current->size >= size + HEADER_SIZE + 4)
            {
                block_header_t* split = (block_header_t*)((uint8_t*)current + HEADER_SIZE + size);
                split->size = current->size - size - HEADER_SIZE;
                split->free = 1;
                split->next = current->next;

                current->size = size;
                current->next = split;
            }

            current->free = 0;
            return (void*)((uint8_t*)current + HEADER_SIZE);
        }

        previous = current;
        current = current->next;
    }

    // No suitable block found, ask PMM for more memory
    block_header_t* block = new_block(size);
    if (!block)
        return 0;

    // Add to chain
    if (!heap_start)
    {
        heap_start = block;
    }
    else
    {
        previous->next = block;
    }

    return (void*)((uint8_t*)block + HEADER_SIZE);
}

void kfree(void* ptr)
{
    if (!ptr)
        return;

    // Step back to find the header
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - HEADER_SIZE);
    block->free = 1;

    // Merge with next block if it's also free (coalescing)
    // This prevents fragmentation over time
    if (block->next && block->next->free)
    {
        block->size += HEADER_SIZE + block->next->size;
        block->next = block->next->next;
    }
}
