#include "shell.h"
#include "vga.h"
#include "string.h"
#include "fat32.h"
#include "kmalloc.h"
#include "rtc.h"
#include "pmm.h"
#include "pit.h"
#include "process.h"
#include "scheduler.h"
#include "elf.h"
#include "paging.h"
#include "keyboard.h"
#include "pic.h"

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

// Defined further down, used by the process-exit handler above it
static void shell_prompt(void);

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
	terminal_writestring("	exec	- Run an ELF program (foreground)\n");
	terminal_writestring("	bg		- Run an ELF program in the background\n");
	terminal_writestring("	ps		- List running processes\n");
	terminal_writestring("	kill	- Terminate a process by PID\n");
	terminal_writestring("	time	- Show current date and time\n");
	terminal_writestring("	uptime	- Show time since boot\n");
	terminal_writestring("	free	- Show free memory\n");
	terminal_writestring("	fasterfetch - Show system info\n");
	terminal_writestring("	reboot	- Restart the machine\n");
	terminal_writestring("	about	- About GordOS\n");
	terminal_writestring("\n");
	terminal_writestring("	Ctrl+L clears the screen, Ctrl+C cancels a line\n");
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

// Load an ELF executable from disk and hand it to the scheduler.
// foreground=1 (exec): the process owns the keyboard and the shell
// waits for it to exit before showing the next prompt.
// foreground=0 (bg): the process runs alongside the shell.
// Returns the new process on success (still valid until this IRQ
// returns), or 0 on failure. (USER_STACK_* come from process.h.)
static process_t* start_program(const char* args, int foreground, const char* who)
{
	if (!args || *args == '\0')
	{
		terminal_writestring(who);
		terminal_writestring(": usage: ");
		terminal_writestring(who);
		terminal_writestring(" FILENAME\n");
		return 0;
	}

	// Read the ELF image into memory (max 64KB for now)
	void* buf = kmalloc(65536);
	if (!buf)
	{
		terminal_writestring(who);
		terminal_writestring(": out of memory\n");
		return 0;
	}

	uint32_t size = 0;
	if (fat32_read_file(args, buf, 65536, &size) != 0)
	{
		terminal_writestring(who);
		terminal_writestring(": file not found\n");
		kfree(buf);
		return 0;
	}

	process_t* proc = process_create();
	if (!proc)
	{
		terminal_writestring(who);
		terminal_writestring(": cannot create process\n");
		kfree(buf);
		return 0;
	}

	uint32_t entry = elf_load(proc, buf, size);
	kfree(buf);

	if (entry == 0)
	{
		terminal_writestring(who);
		terminal_writestring(": not a valid ELF executable\n");
		process_destroy(proc);
		return 0;
	}

	// Map one page of user stack just below the kernel half
	void* stack_phys = pmm_alloc_page();
	if (!stack_phys)
	{
		terminal_writestring(who);
		terminal_writestring(": out of memory\n");
		process_destroy(proc);
		return 0;
	}
	if (paging_map_page_in(proc->page_directory, USER_STACK_PAGE,
	                       (uint32_t)stack_phys,
	                       PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER) != 0)
	{
		terminal_writestring(who);
		terminal_writestring(": out of page tables\n");
		pmm_free_page(stack_phys);
		process_destroy(proc);
		return 0;
	}

	if (foreground)
		keyboard_flush(); // don't leak pre-launch keystrokes to the program

	// Add to the scheduler; it will be time-sliced in. process_destroy
	// is the reaper's job now, not ours.
	process_start(proc, entry, USER_STACK_TOP, foreground);
	return proc;
}

// exec — run a program in the foreground (shell waits for it)
static void cmd_exec(const char* args)
{
	start_program(args, 1, "exec");
}

// bg — run a program in the background (shell stays interactive)
static void cmd_bg(const char* args)
{
	process_t* proc = start_program(args, 0, "bg");
	if (proc)
	{
		terminal_writestring("[");
		print_uint(proc->pid);
		terminal_writestring("] running in background\n");
	}
}

// ps — list scheduled processes
static const char* proc_state_name(uint32_t state)
{
	switch (state)
	{
		case PROCESS_READY:   return "ready";
		case PROCESS_RUNNING: return "running";
		case PROCESS_BLOCKED: return "blocked";
		case PROCESS_DEAD:    return "dead";
		default:              return "?";
	}
}

static void ps_print_one(process_t* p)
{
	print_uint(p->pid);
	if (p->pid == 0)
		terminal_writestring("\t-\tkernel\n");
	else
	{
		terminal_writestring("\t");
		terminal_writestring(p->foreground ? "fg" : "bg");
		terminal_writestring("\t");
		terminal_writestring(proc_state_name(p->state));
		terminal_putchar('\n');
	}
}

static void cmd_ps(void)
{
	terminal_writestring("PID\tTYPE\tSTATE\n");
	scheduler_for_each(ps_print_one);
}

// kill — terminate a process by PID
static void cmd_kill(const char* args)
{
	if (!args || *args == '\0')
	{
		terminal_writestring("Usage: kill PID\n");
		return;
	}

	// Parse a small unsigned PID
	uint32_t pid = 0;
	for (const char* p = args; *p >= '0' && *p <= '9'; p++)
		pid = pid * 10 + (uint32_t)(*p - '0');

	if (pid == 0)
	{
		terminal_writestring("kill: cannot kill the kernel\n");
		return;
	}

	process_t* proc = scheduler_find(pid);
	if (!proc)
	{
		terminal_writestring("kill: no such process\n");
		return;
	}

	if (foreground_process == proc)
		foreground_process = 0;

	if (proc == current_process)
	{
		// We are running on this process's kernel stack (in the
		// keyboard IRQ). Mark it dead; the next scheduler tick switches
		// away and the reaper frees it.
		proc->state = PROCESS_DEAD;
	}
	else
	{
		// Preempted elsewhere: unlink and hand straight to the reaper.
		proc->state = PROCESS_DEAD;
		scheduler_remove(proc);
		process_zombie_add(proc);
	}

	terminal_writestring("killed [");
	print_uint(pid);
	terminal_writestring("]\n");
}

// Called by the reaper (kernel task) when a process has been freed.
// Runs with interrupts briefly disabled so its terminal output can't
// interleave with the keyboard IRQ's shell echo.
void shell_on_process_exit(uint32_t pid, int foreground)
{
	asm volatile("cli");

	if (foreground)
	{
		// The shell was waiting on this program — just bring the
		// prompt back.
		shell_prompt();
	}
	else
	{
		// Background process: announce it, then restore the prompt and
		// whatever the user was in the middle of typing.
		terminal_writestring("\n[");
		print_uint(pid);
		terminal_writestring("] done\n");
		shell_prompt();
		for (int i = 0; i < input_index; i++)
			terminal_putchar(input_buffer[i]);
		for (int i = cursor_pos; i < input_index; i++)
			terminal_cursor_left();
	}

	asm volatile("sti");
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

// reboot — ask the 8042 keyboard controller to pulse the CPU reset line
static void cmd_reboot(void)
{
	terminal_writestring("Rebooting...\n");

	// Drain the 8042 input buffer, then issue the reset command
	uint8_t status;
	do { status = inb(0x64); } while (status & 0x02);
	outb(0x64, 0xFE);

	// If that didn't take, fall back to a triple fault by halting
	asm volatile("cli");
	for (;;) asm volatile("hlt");
}

// ++++++++++++++++++++
// + fasterfetch       +
// ++++++++++++++++++++

static void cpuid(uint32_t leaf, uint32_t* a, uint32_t* b, uint32_t* c, uint32_t* d)
{
	asm volatile("cpuid"
	             : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
	             : "a"(leaf), "c"(0));
}

// Fill name (must hold >= 49 bytes) with the CPU brand string if the
// processor supports it, otherwise the 12-char vendor id.
static void cpu_get_name(char* name)
{
	uint32_t a, b, c, d;

	// Highest supported extended leaf
	cpuid(0x80000000, &a, &b, &c, &d);
	uint32_t max_ext = a;

	if (max_ext >= 0x80000004)
	{
		uint32_t* words = (uint32_t*)name;
		int wi = 0;
		for (uint32_t leaf = 0x80000002; leaf <= 0x80000004; leaf++)
		{
			cpuid(leaf, &a, &b, &c, &d);
			words[wi++] = a;
			words[wi++] = b;
			words[wi++] = c;
			words[wi++] = d;
		}
		name[48] = '\0';

		// Brand strings are often left-padded with spaces; skip them
		// by shifting the string down to the first non-space
		int start = 0;
		while (name[start] == ' ') start++;
		if (start > 0)
		{
			int k = 0;
			while (name[start + k]) { name[k] = name[start + k]; k++; }
			name[k] = '\0';
		}
		return;
	}

	// No brand string: use the 12-character vendor id (EBX, EDX, ECX)
	cpuid(0, &a, &b, &c, &d);
	uint32_t* words = (uint32_t*)name;
	words[0] = b;
	words[1] = d;
	words[2] = c;
	name[12] = '\0';
}

// Left column logo, printed in cyan beside the info lines
static const char* ff_logo[] = {
	"   ________   ",
	"  / GordOS \\  ",
	" |  .----.  | ",
	" |  | >_ |  | ",
	" |  '----'  | ",
	"  \\________/  ",
	"    |    |    ",
	"   /______\\   ",
};
#define FF_LOGO_LINES (int)(sizeof(ff_logo) / sizeof(ff_logo[0]))
#define FF_LOGO_WIDTH 14

static void ff_logo_line(int row)
{
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
	if (row < FF_LOGO_LINES)
		terminal_writestring(ff_logo[row]);
	else
		for (int i = 0; i < FF_LOGO_WIDTH; i++)
			terminal_putchar(' ');
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

static void ff_label(const char* label)
{
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
	terminal_writestring(label);
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

static void cmd_fasterfetch(void)
{
	char cpu_name[49];
	cpu_get_name(cpu_name);

	uint32_t free_pg  = pmm_free_pages();
	uint32_t total_pg = pmm_total_pages();
	uint32_t used_mb  = ((total_pg - free_pg) * 4) / 1024;
	uint32_t total_mb = (total_pg * 4) / 1024;
	uint32_t seconds  = timer_ticks() / 1000;

	int row = 0;

	// Row 0: user@host
	ff_logo_line(row++);
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
	terminal_writestring("gordon");
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	terminal_writestring("@");
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
	terminal_writestring("gordos");
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	terminal_putchar('\n');

	// Row 1: separator
	ff_logo_line(row++);
	terminal_writestring("-------------\n");

	// Row 2: OS
	ff_logo_line(row++);
	ff_label("OS:      ");
	terminal_writestring("GordOS (i686)\n");

	// Row 3: Kernel
	ff_logo_line(row++);
	ff_label("Kernel:  ");
	terminal_writestring("monolithic, higher-half\n");

	// Row 4: Uptime
	ff_logo_line(row++);
	ff_label("Uptime:  ");
	print_uint(seconds / 60);
	terminal_writestring("m ");
	print_uint(seconds % 60);
	terminal_writestring("s\n");

	// Row 5: Shell + Display
	ff_logo_line(row++);
	ff_label("Shell:   ");
	terminal_writestring("gsh\n");

	// Row 6: CPU
	ff_logo_line(row++);
	ff_label("CPU:     ");
	terminal_writestring(cpu_name);
	terminal_putchar('\n');

	// Row 7: Memory
	ff_logo_line(row++);
	ff_label("Memory:  ");
	print_uint(used_mb);
	terminal_writestring(" / ");
	print_uint(total_mb);
	terminal_writestring(" MB\n");

	// Trailing rows: colour palette blocks
	for (int i = 0; i < FF_LOGO_WIDTH; i++)
		terminal_putchar(' ');
	for (int bg = 0; bg < 8; bg++)
	{
		terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, (enum vga_color)bg));
		terminal_writestring("  ");
	}
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	terminal_putchar('\n');

	for (int i = 0; i < FF_LOGO_WIDTH; i++)
		terminal_putchar(' ');
	for (int bg = 8; bg < 16; bg++)
	{
		terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, (enum vga_color)bg));
		terminal_writestring("  ");
	}
	terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
	terminal_putchar('\n');
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

	else if (is_command(input, "bg"))
		cmd_bg(get_args(input, 2));

	else if (is_command(input, "ps"))
		cmd_ps();

	else if (is_command(input, "kill"))
		cmd_kill(get_args(input, 4));

	else if (is_command(input, "uptime"))
		cmd_uptime();

	else if (is_command(input, "free"))
		cmd_free();

	else if (is_command(input, "fasterfetch"))
		cmd_fasterfetch();

	else if (is_command(input, "reboot"))
		cmd_reboot();

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
	// A foreground process owns the keyboard; the keyboard driver
	// routes input to it, not here. This guard is belt-and-suspenders.
	if (foreground_process)
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

		// If that command launched a foreground process, the shell is
		// now waiting on it — the prompt comes back when it exits
		// (shell_on_process_exit), so don't print one here.
		if (!foreground_process)
			shell_prompt();
	}
	else if ((unsigned char)c == KEY_CLEAR)
	{
		// Ctrl+L: wipe the screen but keep whatever is being typed
		extern void terminal_initialize(void);
		terminal_initialize();
		shell_prompt();
		for (int i = 0; i < input_index; i++)
			terminal_putchar(input_buffer[i]);
		for (int i = cursor_pos; i < input_index; i++)
			terminal_cursor_left();
	}
	else if ((unsigned char)c == KEY_CANCEL)
	{
		// Ctrl+C: abandon the current line and start fresh
		terminal_writestring("^C\n");
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
				"rename", "time", "uptime", "free", "exec",
				"bg", "ps", "kill", "fasterfetch", "reboot", 0
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
