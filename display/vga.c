#include <stddef.h>
#include <stdint.h>

#include "vga.h"
#include "string.h"
#include "pic.h"

#define VGA_WIDTH	80
#define VGA_HEIGHT	25
#define VGA_MEMORY	0xB8000

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer = (uint16_t*)VGA_MEMORY;

void update_cursor(void)
{
	uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;

	outb(0x3D4, 0x0F);
	outb(0x3D5, (uint8_t)(pos & 0xFF));
	outb(0x3D4, 0x0E);
	outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

void terminal_initialize(void)
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

	for (size_t y = 0; y < VGA_HEIGHT; y++)
	{
		for (size_t x =0; x < VGA_WIDTH; x++)
		{
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}

	update_cursor();
}

void terminal_setcolor(uint8_t color)
{
	terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void terminal_backspace(void)
{
	if (terminal_column == 0)
		return; // Don't go to previous row

	terminal_column--;
	terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
	update_cursor();
}

static void terminal_scroll(void)
{
	for (size_t y = 0; y < VGA_HEIGHT - 1; y++)
	{
		for (size_t x = 0; x < VGA_WIDTH; x++)
		{
			size_t dst = y * VGA_WIDTH + x;
			size_t src = (y + 1) * VGA_WIDTH + x;
			terminal_buffer[dst] = terminal_buffer[src];
		}
	}

	for (size_t x = 0; x < VGA_WIDTH; x++)
	{
		size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH +x;
		terminal_buffer[index] = vga_entry(' ', terminal_color);
	}

	terminal_row = VGA_HEIGHT - 1;
}


void terminal_putchar(char c)
{
	if (c == '\n')
	{
		terminal_column = 0;
		if (++terminal_row == VGA_HEIGHT)
		{
			terminal_scroll();
		}
	}

	else
	{
		terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
		if (++terminal_column == VGA_WIDTH)
		{
			terminal_column = 0;
			if (++terminal_row == VGA_HEIGHT)
				terminal_scroll();
		}
	}

	update_cursor();
}

void terminal_write(const char* data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data)
{
	terminal_write(data, strlen(data));
}
