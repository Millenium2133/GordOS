#include <stdint.h>
#include <stddef.h>

#include "splash.h"
#include "vga.h"

// Forward declerations from vga.c
void terminal_putchar(char c);
void terminal_writestring(const char* data);
void terminal_selector(uint8_t color);


static inline uint8_t make_color(uint8_t fg, uint8_t bg)
{
	return fg | bg << 4;
}



static void print_border(int width)
{
	terminal_putchar('+');
	for (int i = 0; i < width -2; i++)
		terminal_putchar('-');
	terminal_putchar('+');
	terminal_putchar('\n');
}

// Emply lines with borders to prettyify it
static void print_empty_line(int width)
{
	terminal_putchar('|');
	for (int i = 0; i < width - 2; i++)
		terminal_putchar(' ');
	terminal_putchar('|');
	terminal_putchar('\n');
}


// MORE LINES (With borders)
static void print_bordered_line(const char* str, int width)
{
	int len = 0;
	while (str[len]) len++;

	int inner = width - 2;
	int left_pad = (inner - len) / 2;
	int right_pad = inner - len - left_pad;

	terminal_putchar('|');
	for (int i = 0; i < left_pad; i++) terminal_putchar(' ');
	terminal_writestring(str);
	for (int i = 0; i < right_pad; i++) terminal_putchar(' ');
	terminal_putchar('|');
	terminal_putchar('\n');
}

void splash_show(void)
{
	int box_width = 40;
	int box_indent = (80 - box_width) / 2;

	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));

	for (int i = 0; i < 1; i++)
		terminal_putchar('\n');

	#define INDENT() for (int _i = 0; _i < box_indent; _i++) terminal_putchar(' ')

	INDENT(); print_border(box_width);
	INDENT(); print_empty_line(box_width);
	INDENT(); print_bordered_line("Welcome to GordOS!", box_width);
	INDENT(); print_bordered_line("The OS that sucks", box_width);
	INDENT(); print_bordered_line("(For now :3)", box_width);
	INDENT(); print_bordered_line("Type help to get started", box_width);
	INDENT(); print_empty_line(box_width);
	INDENT(); print_border(box_width);

	#undef INDENT

	// Reset colour (Thats the proper way you spell colour btw)
	terminal_putchar('\n');
	terminal_putchar('\n');

	// VGA_COLOR_LIGHT_GREY is defined in vga.c
	terminal_setcolor(vga_entry_color(7, VGA_COLOR_BLACK));
}
