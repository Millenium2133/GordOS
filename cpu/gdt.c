#include "gdt.h"

#define GDT_ENTRIES 6

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

struct tss_entry
{
	uint32_t prev_tss;
	uint32_t esp0;	// Kernel stack pointer for ring 0
	uint32_t ss0;	// kernel stack segment
	uint32_t esp1;
	uint32_t ss1;
	uint32_t esp2;
	uint32_t ss2;
	uint32_t cr3;
	uint32_t eip;
	uint32_t eflags;
	uint32_t eax, ecx, edx, ebx;
	uint32_t esp, ebp, esi, edi;
	uint32_t es, cs, ss, ds, fs, gs;
	uint32_t ldt;
	uint16_t trap;
	uint16_t iomap_base;
} __attribute__((packed));

static struct tss_entry tss;

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

	// Entry 3
	// User code segment
	gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);

	// Entry 4
	// User data segment
	gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF2, 0xCF);

	// Entry 5
	// TSS segment
	uint32_t tss_base = (uint32_t)&tss;
	uint32_t tss_limit = sizeof(struct tss_entry) - 1;
	gdt_set_entry(5, tss_base, tss_limit, 0x89, 0x00);

	// set up TSS, point esp0 to the top of the kernel stack
	// ss0 = 0x10 (kernel data segment selector)
	tss.ss0 = 0x10;
	tss.esp0 = 0; // We will set this properly when we create a process
	tss.iomap_base = sizeof(struct tss_entry);	

	gdt_flush((uint32_t)&gp);
	asm volatile("ltr %0" : : "r"((uint16_t)0x28));
}

void tss_set_kernel_stack(uint32_t stack)
{
	tss.esp0 = stack;
}