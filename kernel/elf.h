#ifndef ELF_H
#define ELF_H

#include <stdint.h>
#include "process.h"

// ELF magic number
#define ELF_MAGIC 0x464C457F  // 0x7F followed by 'E', 'L', 'F'

// ELF types
#define ET_EXEC 2  // Executable file

// Program header types
#define PT_LOAD 1  // Loadable segment

// ELF header
typedef struct
{
    uint8_t  ident[16];   // Magic number and other info
    uint16_t type;        // Object file type
    uint16_t machine;     // Architecture
    uint32_t version;     // Object file version
    uint32_t entry;       // Entry point virtual address
    uint32_t phoff;       // Program header table file offset
    uint32_t shoff;       // Section header table file offset
    uint32_t flags;       // Processor-specific flags
    uint16_t ehsize;      // ELF header size in bytes
    uint16_t phentsize;   // Program header table entry size
    uint16_t phnum;       // Program header table entry count
    uint16_t shentsize;   // Section header table entry size
    uint16_t shnum;       // Section header table entry count
    uint16_t shstrndx;    // Section name string table index
} __attribute__((packed)) elf_header_t;

// Program header
typedef struct
{
    uint32_t type;    // Segment type
    uint32_t offset;  // Segment file offset
    uint32_t vaddr;   // Segment virtual address
    uint32_t paddr;   // Segment physical address (ignored)
    uint32_t filesz;  // Segment size in file
    uint32_t memsz;   // Segment size in memory
    uint32_t flags;   // Segment flags
    uint32_t align;   // Segment alignment
} __attribute__((packed)) elf_phdr_t;

// Load an ELF binary from a buffer into a process's address space.
// Returns the entry point on success, 0 on failure.
uint32_t elf_load(process_t* proc, void* elf_data, uint32_t elf_size);

#endif