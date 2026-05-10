#include "usermode.h"
#include "gdt.h"
 
// User mode segment selectors
// Each GDT entry is 8 bytes, so:
// Entry 3 (user code) = 3 * 8 = 0x18, with RPL=3 -> 0x1B
// Entry 4 (user data) = 4 * 8 = 0x20, with RPL=3 -> 0x23
#define USER_CODE_SEG 0x1B
#define USER_DATA_SEG 0x23
 
void jump_to_usermode(uint32_t eip, uint32_t esp)
{
	tss_set_kernel_stack(esp);
 
	asm volatile(
		"cli\n"
		"mov %0, %%ax\n"
		"mov %%ax, %%ds\n"
		"mov %%ax, %%es\n"
		"mov %%ax, %%fs\n"
		"mov %%ax, %%gs\n"
		"mov %1, %%ecx\n"       // save user esp in ecx
		"mov %2, %%edx\n"       // save user eip in edx
		"push %0\n"              // ss
		"push %%ecx\n"           // esp
		"pushf\n"
		"pop %%eax\n"
		"or $0x200, %%eax\n"
		"push %%eax\n"           // eflags with IF set
		"push %3\n"              // cs
		"push %%edx\n"           // eip
		"iret\n"
		:
		: "i"(USER_DATA_SEG), "r"(esp), "r"(eip), "i"(USER_CODE_SEG)
		: "eax", "ecx", "edx"
	);
}
 
