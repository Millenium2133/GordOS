#include "pic.h"

// write byte
void outb(uint16_t port, uint8_t value)
{
	asm volatile("outb %0, %1" : : "a"(value),  "Nd"(port));
}

// reab byte
uint8_t inb(uint16_t port)
{
	uint8_t ret;
	asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
	return ret;
}


static void io_wait(void)
{
	outb(0x80, 0);
}

void pic_send_eoi(uint8_t irq)
{
	if (irq >= 8)
		outb(PIC2_COMMAND, PIC_EOI);
	outb(PIC1_COMMAND, PIC_EOI);
}

void pic_remap(void)
{
	//Save current interupt masks incase i fuck up
	uint8_t mask1 = inb(PIC1_DATA);
	uint8_t mask2 = inb(PIC2_DATA);

	//start init
	outb(PIC1_COMMAND, 0x11); io_wait();
	outb(PIC2_COMMAND, 0x11); io_wait();

	//set vector offset
	outb(PIC1_DATA, 0x20); io_wait();
	outb(PIC2_DATA, 0x20); io_wait();

	// Tell PIC's about eachother
	outb(PIC1_DATA, 0x04); io_wait();
	outb(PIC2_DATA, 0x02); io_wait();

	// 8086 mode
	outb(PIC1_DATA, 0x01); io_wait();
	outb(PIC2_DATA, 0x01); io_wait();

	// Restore the saved masks
	outb(PIC1_DATA, mask1);
	outb(PIC2_DATA, mask2);
}
