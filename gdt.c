#include "gdt.h"

#define GDT_ENTRIES 3

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gp;

// declared in gdt _flush.s
//Loads GDRT and reloads segment registers
extern void gdt_flush(uint32_t);

static void gdt_set_entry(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t gran)
{
	gdt[num].base_low = (base & 0xFFFF);
	gdt[num].base_middle = (base >> 16) & 0xFF;
	gdt[num].base_high = (base >> 24) & 0xFF;

	gdt[num].limit_low = (limit & 0xFFFF);
	gdt[num].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
	gdt[num].access = access;
}

void gdt_init(void)
{
	gp.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
	gp.base = (uint32_t)&gdt;

	// Entry 0
	// Null decriptor
	gdt_set_entry(0, 0, 0, 0, 0);

	// Entry 1
	// Kernel code segment
	gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);

	//Entry 2
	// Kernel Daata segment
	gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);

	gdt_flush((uint32_t)&gp);
}
