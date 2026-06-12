#include "serial.h"
#include "pic.h"

#define COM1 0x3F8

// Set once the UART passes its loopback self-test; serial_putchar
// stays a no-op until then so early boot output is safe
static int serial_ready = 0;

int serial_init(void)
{
	outb(COM1 + 1, 0x00);  // disable UART interrupts
	outb(COM1 + 3, 0x80);  // enable DLAB to set the baud divisor
	outb(COM1 + 0, 0x03);  // divisor low byte: 3 = 38400 baud
	outb(COM1 + 1, 0x00);  // divisor high byte
	outb(COM1 + 3, 0x03);  // 8 bits, no parity, 1 stop bit
	outb(COM1 + 2, 0xC7);  // enable FIFO, clear it, 14-byte threshold
	outb(COM1 + 4, 0x0B);  // DTR + RTS + OUT2

	// Loopback self-test: send a byte and check it comes back
	outb(COM1 + 4, 0x1E);
	outb(COM1 + 0, 0xAE);
	if (inb(COM1 + 0) != 0xAE)
		return -1;

	// Back to normal operation
	outb(COM1 + 4, 0x0F);
	serial_ready = 1;
	return 0;
}

static int transmit_empty(void)
{
	return inb(COM1 + 5) & 0x20;
}

void serial_putchar(char c)
{
	if (!serial_ready)
		return;

	// Serial terminals expect CRLF line endings
	if (c == '\n')
		serial_putchar('\r');

	while (!transmit_empty())
		;
	outb(COM1, (uint8_t)c);
}

void serial_writestring(const char* s)
{
	while (*s)
		serial_putchar(*s++);
}
