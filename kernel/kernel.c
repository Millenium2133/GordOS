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


#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002

void kernel_main(uint32_t magic, multiboot_info_t* mbi)
{
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
	process_init();
	scheduler_init();
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

	splash_show();
	shell_init();

	keyboard_init();
	pit_init(1000);

	// Smoke test: create a process with mapped code/stack pages,
	// then tear it down again so nothing is leaked
	process_t* proc = process_create();
	if (proc)
	{
		void* code_phys  = pmm_alloc_page();
		void* stack_phys = pmm_alloc_page();

		paging_map_page_in(proc->page_directory, 0x00100000,
		                   (uint32_t)code_phys,
		                   PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER);

		paging_map_page_in(proc->page_directory, 0xBFFFF000,
		                   (uint32_t)stack_phys,
		                   PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER);

		process_destroy(proc);
		terminal_writestring("Process subsystem OK\n");
	}
	else
	{
		terminal_writestring("Process creation FAILED\n");
	}

	asm volatile("sti");

	for (;;)
		asm volatile("hlt");
}