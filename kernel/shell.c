#include "shell.h"
#include "vga.h"
#include "string.h"
#include "fat32.h"
#include "kmalloc.h"
#include "rtc.h"
#include "pmm.h"
#include "pit.h"
#include "process.h"
#include "elf.h"
#include "paging.h"
#include "keyboard.h"

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

// Print an unsigned integer in decimal
static void print_uint(uint32_t n)
{
	char buf[11];
	int i = 10;
	buf[i] = '\0';
	do
	{
		buf[--i] = '0' + (n % 10);
		n /= 10;
	} while (n);
	terminal_writestring(&buf[i]);
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
	terminal_writestring("	mkdir	- Create directory\n");
	terminal_writestring("	cd		- Change directory\n");
	terminal_writestring("	rm		- Delete a file\n");
	terminal_writestring("	write	- Write text to file\n");
	terminal_writestring("	rename	- Rename a file\n");
	terminal_writestring("	exec	- Run an ELF program in user mode\n");
	terminal_writestring("	time	- Show current date and time\n");
	terminal_writestring("	uptime	- Show time since boot\n");
	terminal_writestring("	free	- Show free memory\n");
	terminal_writestring("	about	- About GordOS\n");
}

// clear
static void cmd_clear(void)
{
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

// ls [path] — no argument lists the current directory
static void cmd_ls(const char* args)
{
	const char* path = (args && *args) ? args : "";
	if (fat32_list_dir(path) != 0)
		terminal_writestring("ls: cannot list directory\n");
}

// cat
static void cmd_cat(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("Usage: cat FILENAME\n");
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

	if (fat32_read_file(args, buf, 65536, &size) != 0)
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

	// Don't truncate a file that already exists
	uint32_t size = 0;
	char probe;
	if (fat32_read_file(args, &probe, 0, &size) == 0)
	{
		terminal_writestring("touch: file already exists\n");
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

// mkdir
static void cmd_mkdir(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("Usage: mkdir DIRNAME\n");
		return;
	}

	if (fat32_mkdir(args) == 0)
	{
		terminal_writestring("Created directory: ");
		terminal_writestring(args);
		terminal_putchar('\n');
	}
	else
		terminal_writestring("mkdir: Failed");
}

// cd
static void cmd_cd(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("Usage: cd DIRNAME\n");
		return;
	}

	if (fat32_cd(args) != 0)
		terminal_writestring("cd: Directory not found\n");
}

// time
static void print_two_digits(uint8_t n)
{
	char buf[3];
	buf[0] = '0' + (n / 10);
	buf[1] = '0' + (n % 10);
	buf[2] = '\0';
	terminal_writestring(buf);
}

static void print_year(uint16_t y)
{
	char buf[5];
	buf[0] = '0' + ((y / 1000) % 10);
	buf[1] = '0' + ((y / 100) % 10);
	buf[2] = '0' + ((y / 10) % 10);
	buf[3] = '0' + (y % 10);
	buf[4] = '\0';
	terminal_writestring(buf);
}

static void cmd_time(void)
{
	rtc_time_t t;
	rtc_read_time(&t);

	print_two_digits(t.hours);
	terminal_putchar(':');
	print_two_digits(t.minutes);
	terminal_putchar(':');
	print_two_digits(t.seconds);
	terminal_writestring("  ");
	print_year(t.year);
	terminal_putchar('-');
	print_two_digits(t.month);
	terminal_putchar('-');
	print_two_digits(t.day);
	terminal_putchar('\n');
}

// exec — load an ELF executable from disk and run it in user mode.
// Blocks until the program calls sys_exit.
#define USER_STACK_PAGE 0xBFFFF000
#define USER_STACK_TOP  0xBFFFFFF0

static void cmd_exec(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("Usage: exec FILENAME\n");
		return;
	}

	// Read the ELF image into memory (max 64KB for now)
	void* buf = kmalloc(65536);
	if (!buf)
	{
		terminal_writestring("exec: out of memory\n");
		return;
	}

	uint32_t size = 0;
	if (fat32_read_file(args, buf, 65536, &size) != 0)
	{
		terminal_writestring("exec: file not found\n");
		kfree(buf);
		return;
	}

	process_t* proc = process_create();
	if (!proc)
	{
		terminal_writestring("exec: cannot create process\n");
		kfree(buf);
		return;
	}

	uint32_t entry = elf_load(proc, buf, size);
	kfree(buf);

	if (entry == 0)
	{
		terminal_writestring("exec: not a valid ELF executable\n");
		process_destroy(proc);
		return;
	}

	// Map one page of user stack just below the kernel half
	void* stack_phys = pmm_alloc_page();
	if (!stack_phys)
	{
		terminal_writestring("exec: out of memory\n");
		process_destroy(proc);
		return;
	}
	paging_map_page_in(proc->page_directory, USER_STACK_PAGE,
	                   (uint32_t)stack_phys,
	                   PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER);

	// Don't let the program see keystrokes from before it started
	keyboard_flush();

	// Runs the program in ring 3, returns when it exits
	process_run(proc, entry, USER_STACK_TOP);
	process_destroy(proc);
}

// uptime
static void cmd_uptime(void)
{
	uint32_t seconds = timer_ticks() / 1000;
	terminal_writestring("Up ");
	print_uint(seconds / 60);
	terminal_writestring(" min ");
	print_uint(seconds % 60);
	terminal_writestring(" sec\n");
}

// free
static void cmd_free(void)
{
	uint32_t pages = pmm_free_pages();
	print_uint(pages);
	terminal_writestring(" pages free (");
	print_uint(pages * 4);
	terminal_writestring(" KB)\n");
}


// ++++++++++++++++++++
// + Command Dispatch +
// ++++++++++++++++++++

// Returns 1 if the first word of input is exactly cmd
// (followed by a space or end of string)
static int is_command(const char* input, const char* cmd)
{
	size_t len = strlen(cmd);
	if (strncmp(input, cmd, len) != 0)
		return 0;
	return input[len] == '\0' || input[len] == ' ';
}

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

	if (is_command(input, "help"))
		cmd_help();

	else if (is_command(input, "clear"))
		cmd_clear();

	else if (is_command(input, "echo"))
		cmd_echo(get_args(input, 4));

	else if (is_command(input, "about"))
		cmd_about();

	else if (is_command(input, "ls"))
		cmd_ls(get_args(input, 2));

	else if (is_command(input, "cat"))
		cmd_cat(get_args(input, 3));

	else if (is_command(input, "touch"))
		cmd_touch(get_args(input, 5));

	else if (is_command(input, "write"))
		cmd_write(get_args(input, 5));

	else if (is_command(input, "rm"))
		cmd_rm(get_args(input, 2));

	else if (is_command(input, "rename"))
		cmd_rename(get_args(input, 6));

	else if (is_command(input, "pwd"))
		cmd_pwd();

	else if (is_command(input, "mkdir"))
		cmd_mkdir(get_args(input, 5));

	else if (is_command(input, "cd"))
		cmd_cd(get_args(input, 2));

	else if (is_command(input, "time"))
		cmd_time();

	else if (is_command(input, "exec"))
		cmd_exec(get_args(input, 4));

	else if (is_command(input, "uptime"))
		cmd_uptime();

	else if (is_command(input, "free"))
		cmd_free();

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
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	terminal_writestring(fat32_get_cwd_path());
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
	// While a user process is running, the shell is suspended inside
	// cmd_exec — running another command from this IRQ context would
	// clobber the exec state, so drop the input
	if (current_process)
		return;

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
	else if ((unsigned char)c == KEY_TAB)
	{
		// Extract current word being typed (from last space to cursor)
		char prefix[INPUT_BUFFER_SIZE];
		int pi = 0;
		int word_start = cursor_pos;
		while (word_start > 0 && input_buffer[word_start - 1] != ' ')
			word_start--;
		for (int i = word_start; i < cursor_pos; i++)
			prefix[pi++] = input_buffer[i];
		prefix[pi] = '\0';

		// Check if we are still on the command word (no space before cursor)
		int has_space = 0;
		for (int i = 0; i < cursor_pos; i++)
		{
			if (input_buffer[i] == ' ') { has_space = 1; break; }
		}

		char matches[16][64];
		int count = 0;

		if (!has_space)
		{
			// Complete command names from the built-in list
			static const char* commands[] = {
				"help", "clear", "echo", "about", "ls", "pwd",
				"cat", "touch", "mkdir", "cd", "rm", "write",
				"rename", "time", "uptime", "free", "exec", 0
			};
			for (int ci = 0; commands[ci] != 0 && count < 16; ci++)
			{
				// Check if command starts with prefix
				int match = 1;
				for (int j = 0; j < pi; j++)
				{
					if (commands[ci][j] == '\0' || commands[ci][j] != prefix[j])
					{
						match = 0;
						break;
					}
				}
				if (match)
				{
					int j;
					for (j = 0; commands[ci][j] && j < 12; j++)
						matches[count][j] = commands[ci][j];
					matches[count][j] = '\0';
					count++;
				}
			}
		}
		else
		{
			// Complete filenames/directories from the filesystem
			count = fat32_find_prefix(prefix, matches, 16);
		}

		if (count == 1)
		{
			// Find end of current word in buffer
			int word_end = cursor_pos;
			while (word_end < input_index && input_buffer[word_end] != ' ')
				word_end++;

			int old_len = word_end - word_start;
			int new_len = 0;
			while (matches[0][new_len]) new_len++;
			int diff = new_len - old_len;

			// Shift buffer contents to accommodate new length
			if (diff > 0)
			{
				for (int i = input_index; i >= word_end; i--)
					input_buffer[i + diff] = input_buffer[i];
			}
			else if (diff < 0)
			{
				for (int i = word_end + diff; i <= input_index; i++)
					input_buffer[i] = input_buffer[i - diff];
			}

			// Write completed name into buffer
			for (int i = 0; i < new_len; i++)
				input_buffer[word_start + i] = matches[0][i];

			input_index += diff;
			cursor_pos = word_start + new_len;

			// Move terminal cursor back to word_start so we redraw
			// over the prefix text, not after it
			for (int i = word_start; i < word_end; i++)
				terminal_cursor_left();

			// Redraw from word_start to end, plus trailing space to
			// erase any leftover characters if new name is shorter
			for (int i = word_start; i < input_index; i++)
				terminal_putchar(input_buffer[i]);
			terminal_putchar(' ');

			// Move hardware cursor back to correct position
			for (int i = cursor_pos; i < input_index + 1; i++)
				terminal_cursor_left();
		}
		else if (count > 1)
		{
			// Print all matches on a new line
			terminal_putchar('\n');
			for (int i = 0; i < count; i++)
			{
				terminal_writestring(matches[i]);
				terminal_putchar(' ');
			}
			terminal_putchar('\n');

			// Reprint prompt and current buffer contents
			shell_prompt();
			for (int i = 0; i < input_index; i++)
				terminal_putchar(input_buffer[i]);

			// Move cursor back to correct position if not at end
			for (int i = cursor_pos; i < input_index; i++)
				terminal_cursor_left();
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
		if (history_index < 0)
			return; // not navigating history, leave typed input alone

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
