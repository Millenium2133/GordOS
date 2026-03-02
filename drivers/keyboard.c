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

static  const char scancode_table_shift[128] =
{
    0,   0,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
    0,  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,  ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

static int shift_pressed = 0;

void terminal_putchar(char c);
void terminal_backspace(void);

static int extended = 0;


static void  keyboard_handler(struct registers regs)
{
	(void)regs;

	// Extended scancode prefix
	uint8_t scancode = inb(0x60);

	if (scancode == 0xE0)
	{
		extended = 1;
		return;
	}

	// Key release
	if (scancode & 0x80)
	{
		uint8_t released = scancode & 0x7F;
		if (!extended && (released == 0x2A || released == 0x36))
			shift_pressed = 0;
		extended = 0;
		return;
	}

	
	// If extended scancode
	if (extended)
	{
		extended = 0;
		return;
	}

	// Shift, Enter and "Backspace"
	// A better backspace will come with the shell once that is made
	if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
	if (scancode == 0x1C) { terminal_putchar('\n'); return; }
	if (scancode == 0x0E) { terminal_backspace(); return; }

	char c = shift_pressed ? scancode_table_shift[scancode] : scancode_table[scancode];
	if (c != 0)
		terminal_putchar(c);
}

void keyboard_init(void)
{
	irq_register(1, keyboard_handler);
}
