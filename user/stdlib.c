// Userland malloc/free, ported from memory/kmalloc.c but backed by sys_sbrk
// instead of pmm_alloc_contiguous. Same coalescing free-list design:
// A header sits in front of each allocation, malloc splits a free block is theres
// enough room left over, free merges forward with adjacent free block.
//
// This file is self-contained (own syscall stub, like every other user program)
// since there's no shared libc yet.
// See the README's Phase 1 Groundwork notes.

#include "malloc.h"
#include <stdint.h>

#define SYS_SBRK 35
#define PAGE_SIZE 4096

typedef struct block_header
{
    size_t size;                // Size of the data block (not including the header)
    int free;                   // 1 = free, 0 = in use
    struct block_header* next; // Next free block in the chain
} block_header_t;

#define HEADER_SIZE sizeof(block_header_t)

static block_header_t* heap_head = 0;

static inline int syscall1(int num, int arg1)
{
    int ret;
    __asm__ volatile("int $0x80"
                     : "=a"(ret)
                     : "a"(num), "b"(arg1)
                     : "memory");
    return ret;
}

// Ask the kernel to grow our heap by 'increment' bytes. Returns the
// start of the newly useable region, or 0 on failure.
static void* do_sbrk(int increment)
{
    int ret = syscall1(SYS_SBRK, increment);
    if (ret == -1)
        return (void*)0;
    return (void*)(uint32_t)ret;
}

// Ask sys_sbrk for enough whole pages to cover size + a header, and
// turn the result into a block.
static block_header_t* new_block(size_t size)
{
    size_t pages_needed = (size + HEADER_SIZE + PAGE_SIZE - 1) / PAGE_SIZE;
    size_t bytes = pages_needed * PAGE_SIZE;

    void* mem = do_sbrk((int)bytes);
    if (!mem)
        return 0;
    
    block_header_t* block = (block_header_t*)mem;
    block->size = bytes - HEADER_SIZE;
    block->free = 0;
    block->next = 0;
    return block;

}

void* malloc(size_t size)
{
    if (size == 0)
        return 0;

    // Align size to 4 bytes so allocations are aligned
    size = (size + 3) & ~3;

    // Walk the chain looking for a free block that is big enough
    block_header_t* current = heap_head;
    block_header_t* previous = 0;

    while (current)
    {
        if (current->free && current->size >= size)
        {
            // Found a fit. Split it if theres enough room left over
            // for another heasder and at least 4 bytes of data.
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

    // No sutiable block found, ask sys_sbrk for more memory
    block_header_t* block = new_block(size);
    if (!block)
        return 0;

    if (!heap_head)
    {
        heap_head = block;
    }
    else
    {
        previous->next = block;
    }

    return (void*)((uint8_t*)block + HEADER_SIZE);
}

void free(void* ptr)
{
    if (!ptr)
        return;

    // Step back to find the header
    block_header_t* block = (block_header_t*)((uint8_t*)ptr - HEADER_SIZE);
    block->free = 1;

    // Merge with next block if it's also free (coalescing). Only merge if the
    // blocks are physically adjacent, seperate sbrk-grown blocks may have gaps
    // if a future version frees memory back to the kernel (this one never does, 
    // but keep the check for safety and to mirror kmalloc.c).
    if (block->next && block->next->free && (uint8_t*)block + HEADER_SIZE + block->size == (uint8_t*)block->next)
    {
        block->size += HEADER_SIZE + block->next->size;
        block->next = block->next->next;
    }
}