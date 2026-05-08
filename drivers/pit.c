#include "pit.h"
#include "pic.h"
#include "idt.h"

#define PIT_CHANNEL0	0x40
#define PIT_COMMAND	0x43
#define PIT_BASE_HZ	1193182

static volatile uint32_t ticks = 0;
static uint32_t tick_rate = 0;

static void pit_handler(struct registers regs)
{
	(void)regs;
	ticks++;
}

void pit_init(uint32_t frequency)
{
	tick_rate = frequency;

	// Calculate the divisor. the PIT counts down from this value
	// at its base frequency of 1,193,182 Hz
	uint32_t divisor = PIT_BASE_HZ / frequency;

	// Command byte: channel 0, lobyte/hibyte access, square wave mode
	outb(PIT_COMMAND, 0x36);
	outb(PIT_CHANNEL0, (uint8_t)(divisor & 0xFF));
	outb(PIT_CHANNEL0, (uint8_t)((divisor >> 8) & 0xFF));

	irq_register(0, pit_handler);
}

uint32_t timer_ticks(void)
{
	return ticks;
}


void timer_sleep(uint32_t ms)
{
	uint32_t ticks_needed = (ms * tick_rate) / 1000;
	uint32_t start = ticks;
	while (ticks - start < ticks_needed)
		asm volatile("hlt");
}
