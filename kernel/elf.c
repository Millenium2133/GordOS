#include "elf.h"
#include "paging.h"
#include "pmm.h"
#include "kmalloc.h"

// Simple memcpy since we don't have the standard library
static void elf_memcpy(uint8_t* dst, uint8_t* src, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        dst[i] = src[i];
}

static void elf_memzero(uint8_t* dst, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; i++)
        dst[i] = 0;
}

uint32_t elf_load(process_t* proc, void* elf_data, uint32_t elf_size)
{
    if (!proc || !elf_data || elf_size < sizeof(elf_header_t))
        return 0;

    elf_header_t* header = (elf_header_t*)elf_data;

    // Check magic number
    uint32_t magic = *(uint32_t*)header->ident;
    if (magic != ELF_MAGIC)
        return 0;

    // Must be a 32-bit executable
    if (header->type != ET_EXEC)
        return 0;

    // Walk program headers and load PT_LOAD segments
    uint32_t i;
    for (i = 0; i < header->phnum; i++)
    {
        elf_phdr_t* phdr = (elf_phdr_t*)((uint8_t*)elf_data + header->phoff + i * header->phentsize);

        if (phdr->type != PT_LOAD)
            continue;

        if (phdr->memsz == 0)
            continue;

        // Calculate how many pages this segment needs
        uint32_t pages_needed = (phdr->memsz + 0xFFF) / 0x1000;
        uint32_t vaddr = phdr->vaddr & ~0xFFF;  // align down to page boundary

        // Allocate and map pages for this segment
        uint32_t page;
        for (page = 0; page < pages_needed; page++)
        {
            void* phys = pmm_alloc_page();
            if (!phys)
                return 0;

            uint32_t virt = vaddr + page * 0x1000;
            paging_map_page_in(proc->page_directory, virt, (uint32_t)phys,
                               PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER);

            #define SCRATCH_VIRT 0xC03FF000
            paging_map_page(SCRATCH_VIRT, (uint32_t)phys, PAGE_PRESENT | PAGE_WRITEABLE);

            uint8_t* scratch = (uint8_t*)SCRATCH_VIRT;

            // Zero the page first
            elf_memzero(scratch, 0x1000);

            // Copy file data if this page overlaps with the file portion
            uint32_t page_start = page * 0x1000;
            uint32_t page_end   = page_start + 0x1000;
            uint32_t file_start = phdr->vaddr - vaddr;  // offset within segment
            uint32_t file_end   = file_start + phdr->filesz;

            if (page_start < file_end && page_end > file_start)
            {
                uint32_t copy_start = page_start > file_start ? page_start : file_start;
                uint32_t copy_end   = page_end < file_end ? page_end : file_end;
                uint32_t copy_len   = copy_end - copy_start;

                uint8_t* src = (uint8_t*)elf_data + phdr->offset + (copy_start - file_start);
                uint8_t* dst = scratch + (copy_start - page_start);

                elf_memcpy(dst, src, copy_len);
            }

            // Remove the scratch mapping
            paging_map_page(SCRATCH_VIRT, 0, 0);
            asm volatile("invlpg (%0)" : : "r"(SCRATCH_VIRT) : "memory");
        }
    }

    return header->entry;
}