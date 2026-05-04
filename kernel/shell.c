#include "shell.h"
#include "vga.h"
#include "string.h"
#include "fat32.h"
#include "kmalloc.h"

#define INPUT_BUFFER_SIZE 256

static char input_buffer[INPUT_BUFFER_SIZE];
static int input_index = 0;

#define HISTORY_SIZE 10
static char history[HISTORY_SIZE][INPUT_BUFFER_SIZE];
static int history_count = 0;
static int history_index = -1;
static int cursor_pos = 0;

// Forward declarations from vga.c
void terminal_writestring(const char* data);
void terminal_putchar(char c);
void terminal_backspace(void);
void terminal_setcolor(uint8_t color);
void terminal_cursor_left(void);

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
	terminal_writestring("	ls		- List files in directory\n");
	terminal_writestring("	pwd		- Print working directory\n");
	terminal_writestring("	cat		- Print file contents\n");
	terminal_writestring("	touch	- Create empty file\n");
	terminal_writestring("	rm		- Delete a file\n");
	terminal_writestring("	write	- Write text to file\n");
	terminal_writestring("	rename	- Rename a file\n");
	terminal_writestring("	about	- About GordOS\n");
}

// clear
static void cmd_clear(void)
{
	for (int y = 0; y < 25; y++)
		for (int x = 0; x < 80; x++)
			terminal_putchar(' ');

	extern void terminal_initialize(void);
	terminal_initialize();
}

// echo
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

// about
static void cmd_about(void)
{
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
	terminal_writestring("	GordOS\n");
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	terminal_writestring("	A hobby OS by Hamish Gordon\n");
	terminal_writestring("	Built from scratch in C and x86 Assembly\n");
	terminal_writestring("	https://github.com/Millenium2133/GordOS\n");
}

// ls
static void cmd_ls(void)
{
	if (fat32_list_dir("/") != 0)
		terminal_writestring("ls: filesystem not available\n");
}

// cat
static void cmd_cat(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("Usage: car FILENAME\n");
		return;
	}

	// Allocate a buffer for the file (max 64KB for now)
	uint32_t size = 0;
	void* buf = kmalloc(65536);
	if (!buf)
	{
		terminal_writestring("cat: out of memory\n");
		return;
	}

	if (fat32_read_file(args, buf, &size) != 0)
	{
		terminal_writestring("cat: file not found\n");
		kfree(buf);
		return;
	}

	//print the fine contents character by character
	char* data = (char*)buf;
	for (uint32_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
	terminal_putchar('\n');

	kfree(buf);
}

// touch
static void cmd_touch(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("Usage: touch FILENAME\n");
		return;
	}

	if (fat32_write_file(args, "", 0) == 0)
	{
		terminal_writestring("Created: ");
		terminal_writestring(args);
		terminal_putchar('\n');
	}
}

// write
static void cmd_write(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("usage: write FILENAME TEXT\n");
		return;
	}

	// Split filename and content
	const char* content = args;
	int fi = 0;
	char filename[13];

	while (*content && *content != ' ' && fi < 12)
		filename[fi++] = *content++;
	filename[fi] = '\0';

	if (*content == ' ')
		content++;

	if (fat32_write_file(filename, content, strlen(content)) == 0)
	{
		terminal_writestring("Written to: ");
		terminal_writestring(filename);
		terminal_putchar('\n');
	}
	else
		terminal_writestring("write: failed\n");
}

// rm
static void cmd_rm(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("Usage: rm FILENAME\n");
		return;
	}

	if (fat32_delete_file(args) == 0)
	{
		terminal_writestring("Deleted: ");
		terminal_writestring(args);
		terminal_putchar('\n');
	}
	else
		terminal_writestring("rm: file not found\n");
}

// rename
static void cmd_rename(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("Usage: rename OLDNAME NEWNAME\n");
		return;
	}

	// split into two filenames
	const char * second = args;
	int fi = 0;
	char oldname[13];

	while (*second && *second != ' ' && fi < 12)
		oldname[fi++] = *second++;
	oldname[fi] = '\0';

	if (!second || *second == '\0')
	{
		terminal_writestring("rename: missing new filename\n");
		return;
	}

	if (*second == ' ')
		second++;

	if (fat32_rename_file(oldname, second) == 0)
	{
		terminal_writestring("Renamed to: ");
		terminal_writestring(second);
		terminal_putchar('\n');
	}
	else
		terminal_writestring("rename: file not found\n");
}

static void cmd_pwd(void)
{
	terminal_writestring(fat32_get_cwd_path());
	terminal_putchar('\n');
}

// ++++++++++++++++++++
// + Command Dispatch +
// ++++++++++++++++++++

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

	else if (shell_strcmp(input, "ls") == 0)
		cmd_ls();

	else if (shell_strncmp(input, "cat", 3) == 0)
		cmd_cat(get_args(input, 3));

	else if (shell_strncmp(input, "touch", 5) == 0)
		cmd_touch(get_args(input, 5));

	else if (shell_strncmp(input, "write", 5) == 0)
		cmd_write(get_args(input, 5));

	else if (shell_strncmp(input, "rm", 2) == 0)
		cmd_rm(get_args(input, 2));

	else if (shell_strncmp(input, "rename", 6) == 0)
		cmd_rename(get_args(input, 6));

	else if (shell_strcmp(input, "pwd") == 0)
		cmd_pwd();

	else
	{
		terminal_writestring("Unknown Command: ");
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

// Moves cursor to end of input, then erases the whole line
static void clear_input_line(void)
{
	// First move cursor to end of line
	for (int i = cursor_pos; i < input_index; i++)
		terminal_putchar(input_buffer[i]);
	// Then backspace the whole thing
	for (int i = 0; i < input_index; i++)
		terminal_backspace();
}

void shell_handle_char(char c)
{
	if (c == '\n')
	{
		terminal_putchar('\n');
		input_buffer[input_index] = '\0';

		// Save to history if non-empty
		if (input_index > 0)
		{
			if (history_count == HISTORY_SIZE)
			{
				for (int i = 0; i < HISTORY_SIZE - 1; i++)
					for (int j = 0; j < INPUT_BUFFER_SIZE; j++)
						history[i][j] = history[i + 1][j];
				history_count--;
			}
			for (int i = 0; i <= input_index; i++)
				history[history_count][i] = input_buffer[i];
			history_count++;
		}

		shell_execute(input_buffer);
		input_index = 0;
		cursor_pos = 0;
		history_index = -1;
		shell_prompt();
	}
	else if (c == '\b')
	{
		if (cursor_pos > 0)
		{
			// Shift buffer left
			for (int i = cursor_pos - 1; i < input_index - 1; i++)
				input_buffer[i] = input_buffer[i + 1];
			input_index--;
			cursor_pos--;

			// Move cursor back one, then redraw from there to end
		        terminal_cursor_left();
       			 for (int i = cursor_pos; i < input_index; i++)
           			 terminal_putchar(input_buffer[i]);
       			 terminal_putchar(' ');  // erase the now-dangling last char

			// Move cursor back to correct position
			for (int i = cursor_pos; i < input_index + 1; i++)
				terminal_cursor_left();
		}
	}
	else if ((unsigned char)c == KEY_LEFT)
	{
		if (cursor_pos > 0)
		{
			cursor_pos--;
			terminal_cursor_left();
		}
	}
	else if ((unsigned char)c == KEY_RIGHT)
	{
		if (cursor_pos < input_index)
		{
			terminal_putchar(input_buffer[cursor_pos]);
			cursor_pos++;
		}
	}
	else if ((unsigned char)c == KEY_UP)
	{
		int new_index = history_index + 1;
		if (new_index < history_count)
		{
			history_index = new_index;
			clear_input_line();
			int entry = history_count - 1 - history_index;
			input_index = 0;
			cursor_pos = 0;
			for (int i = 0; history[entry][i] != '\0'; i++)
			{
				input_buffer[input_index++] = history[entry][i];
				terminal_putchar(history[entry][i]);
				cursor_pos++;
			}
		}
	}
	else if ((unsigned char)c == KEY_DOWN)
	{
		history_index--;
		if (history_index < 0)
		{
			history_index = -1;
			clear_input_line();
			input_index = 0;
			cursor_pos = 0;
		}
		else
		{
			clear_input_line();
			int entry = history_count - 1 - history_index;
			input_index = 0;
			cursor_pos = 0;
			for (int i = 0; history[entry][i] != '\0'; i++)
			{
				input_buffer[input_index++] = history[entry][i];
				terminal_putchar(history[entry][i]);
				cursor_pos++;
			}
		}
	}
	else if (input_index < INPUT_BUFFER_SIZE - 1)
	{
		// Insert character at cursor position
		for (int i = input_index; i > cursor_pos; i--)
			input_buffer[i] = input_buffer[i - 1];
		input_buffer[cursor_pos] = c;
		input_index++;

		// Redraw from cursor to end
		for (int i = cursor_pos; i < input_index; i++)
			terminal_putchar(input_buffer[i]);
		cursor_pos++;

		// Move cursor back to correct position
		for (int i = cursor_pos; i < input_index; i++)
			terminal_cursor_left();
	}
}

void shell_init(void)
{
	input_index = 0;
	cursor_pos = 0;
	history_index = -1;
	shell_prompt();
}
