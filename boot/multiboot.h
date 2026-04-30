#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

#define MULTIBOOT_MAGIC 0x2BADB002
#define MULTIBOOT_FLAG_MEM 0x001
#define MULTIBOOT_FLAG_MMAP 0x040

typedef struct
{
    uint32_t flags;
    uint32_t mem_lower;      // KB of lower memory (below 1MB)
    uint32_t mem_upper;      // KB of upper memory (above 1MB)
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t syms[4];
    uint32_t mmap_length;    // size of the memory map in bytes
    uint32_t mmap_addr;      // physical address of the memory map
} __attribute__((packed)) multiboot_info_t;

typedef struct
{
    uint32_t size;           // size of this entry (not including this field)
    uint32_t addr_low;       // physical address (low 32 bits)
    uint32_t addr_high;      // physical address (high 32 bits)
    uint32_t len_low;        // length in bytes (low 32 bits)
    uint32_t len_high;       // length in bytes (high 32 bits)
    uint32_t type;           // 1 = usable RAM, anything else = reserved
} __attribute__((packed)) multiboot_mmap_t;

#endif
