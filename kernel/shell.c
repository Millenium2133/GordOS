#include "shell.h"
#include "vga.h"
#include "string.h"

#define INPUT_BUFFER_SIZE 256

static char input_buffer[INPUT_BUFFER_SIZE];
static int input_index = 0;

// Forward declareations form vga.c
void terminal_writestring(const char* data);
void terminal_putchar(char c);
void terminal_backspace(void);
void terminal_setcolor(uint8_t color);

// forward dec from vga.h
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg);

// ++++++++++++++++++++
// + String Utilities +
// ++++++++++++++++++++

static int shell_strcmp(const char* a, const char* b)
{
	while (*a && *b && *a == *b) { a++; b++; }
	return *a - *b;
}

static int shell_strncmp(const char* a, const char* b, size_t n)
{
	for (size_t i = 0; i < n; i++)
	{
		if (a[i] != b[i]) return a[i] - b[i];
		if (a[i] == '\0') return 0;
	}
	return 0;
}

//++++++++++++
//+ Commands +
//++++++++++++

// help
static void cmd_help(void)
{
	terminal_writestring("Available Commands\n");
	terminal_writestring("	help	- Shows this message\n");
	terminal_writestring("	clear	- Clears screen\n");
	terminal_writestring("	echo	- Print text to screen\n");
	terminal_writestring("	about	- About GordOS\n");
}

// clear
static void cmd_clear(void)
{
	// Clear by writing spaces to every cell
	for (int y = 0; y < 25; y++);
		for (int x = 0; x < 80; x++)
			terminal_putchar(' ');

	// reset cursor to  top by reinitializing terminal
	extern void terminal_initialize(void);
	terminal_initialize();
}

//echo
static void cmd_echo(const char* args)
{
	if (args == 0 || *args == '\0')
	{
		terminal_putchar('\n');
		return;
	}

	terminal_writestring(args);
	terminal_putchar('\n');
}

// About
static void cmd_about(void)
{
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
	terminal_writestring("	GordOS\n");
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	terminal_writestring("	A hobby OS by Hamish Gordon\n");
	terminal_writestring("	Built from scratch in C and x86 Assembly\n");
	terminal_writestring("	https://github.io/Millenium2133/GordOS\n");
}

// ++++++++++++++++++++
// + Command Dispatch +
// ++++++++++++++++++++

//simple Tokenizer
static const char* get_args(const char* input, size_t cmd_len)
{
	if (input[cmd_len] == ' ' && input[cmd_len + 1] != '\0')
		return &input[cmd_len + 1];
	return 0;
}

static void shell_execute(const char* input)
{
	if (*input == '\0')
		return;

	if (shell_strcmp(input, "help") == 0)
		cmd_help();
	else if (shell_strcmp(input, "clear") == 0)
		cmd_clear();
	else if (shell_strncmp(input, "echo", 4) == 0)
		cmd_echo(get_args(input, 4));
	else if (shell_strcmp(input, "about") == 0)
		cmd_about();
	else
	{
		terminal_writestring("Unknown Commant: ");
		terminal_writestring(input);
		terminal_putchar('\n');
	}
}

// ++++++++++++++++++
// + Input Handling +
// ++++++++++++++++++

static void shell_prompt(void)
{
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
	terminal_writestring("GordOS");
	terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
	terminal_writestring("> ");
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

void shell_handle_char(char c)
{
	if (c == '\n')
	{
		terminal_putchar('\n');
		input_buffer[input_index] = '\0';
		shell_execute(input_buffer);
		input_index = 0;
		shell_prompt();
	}
	else if (c == '\b')
	{
		if (input_index > 0)
		{
			input_index--;
			terminal_backspace();
		}
	}
	else if (input_index < INPUT_BUFFER_SIZE - 1)
	{
		input_buffer[input_index++] = c;
		terminal_putchar(c);
	}
}

void shell_init(void)
{
	shell_prompt();
}



















