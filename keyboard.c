#include "keyboard.h"
#include "idt.h"
#include "pic.h"

static const char scancode_table[128] =
{
    0,   0,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',  0,
    0,  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',  0,
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,  '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',  0,
    '*',  0,  ' '
};


void terminal_putchar(char c);


static void  keyboard_handler(struct registers regs)
{
	(void)regs;

	uint8_t scancode = inb(0x60);

	if (scancode & 0x80)
		return;

	char c = scancode_table[scancode];

	if (c != 0)
		terminal_putchar(c);
}

void keyboard_init(void)
{
	irq_register(1, keyboard_handler);
}
