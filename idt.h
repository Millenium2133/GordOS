#ifndef IDT_H
#define IDT_H

#include <stdint.h>

struct registers
{
	uint32_t ds;
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
	uint32_t int_no, err_code;
	uint32_t eip, cs, eflags, useresp, ss;
};

void irq_register(int irq, void (*handler)(struct registers));

struct idt_entry
{
	uint16_t base_low;
	uint16_t selector;
	uint8_t always0;
	uint8_t flags;
	uint16_t base_high;
}__attribute__((packed));

struct idt_ptr
{
	uint16_t limit;
	uint32_t base;
}__attribute__((packed));

void idt_init(void);

#endif
