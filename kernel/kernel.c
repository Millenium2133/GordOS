#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Handcrafted dependencies
#include "vga.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "keyboard.h"
#include "string.h"
#include "splash.h"
#include "shell.h"
#include "multiboot.h"
#include "pmm.h"
#include "kmalloc.h"
#include "ata.h"
#include "fat32.h"
#include "paging.h"
#include "pit.h"
#include "syscall.h"
#include "usermode.h"
#include "process.h"
#include "scheduler.h"
#include "elf.h"
#include "serial.h"


#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, multiboot_info_t* mbi)
{
	serial_init(); // first, so all terminal output gets mirrored
	terminal_initialize();
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC)
		terminal_writestring("WARNING: bad multiboot magic, memory map may be invalid\n");
	gdt_init();
	pic_remap();
	idt_init();
	syscall_init();
	pmm_init(mbi);
	paging_init();
	kmalloc_init();
	scheduler_init();   // must precede process_init: it registers the
	process_init();     // kernel task (pid 0) with the scheduler
	terminal_writestring("PMM Initialized\n");

	if (ata_init() == 0)
	{
		terminal_writestring("ATA drive DETECTED\n");
		if (fat32_init() == 0)
			terminal_writestring("FAT32 MOUNTED\n");
		else
			terminal_writestring("FAT32 mount FAILED\n");
	}
	else
		terminal_writestring("ATA init FAILED\n");

	void* a = kmalloc(64);
	void* b = kmalloc(128);
	kfree(a);
	void* c = kmalloc(32);

	if (a && b && c)
		terminal_writestring("kmalloc OK\n");
	else
		terminal_writestring("kmalloc FAILED\n");

	terminal_writestring("Scheduler ready\n");

	splash_show();
	shell_init();

	keyboard_init();
	pit_init(1000);

	asm volatile("sti");

	// This is the kernel task (pid 0): reap exited processes, then
	// idle. The shell runs from the keyboard IRQ; user processes are
	// time-sliced in by the scheduler.
	for (;;)
	{
		process_reap();
		asm volatile("hlt");
	}
}