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

// Compiler check
#if defined(__linux__)
#error "You are not using a cross compiler"
#endif

#if !defined(__i386__)
#error "This kernel needs to be compiled with an ix86-elf compiler"
#endif

void kernel_main(uint32_t magic, multiboot_info_t* mbi)
{
	terminal_initialize();
	(void)magic;
	gdt_init();
	pic_remap();
	idt_init();
	pmm_init(mbi);
	kmalloc_init();
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


	// Sanity check for kmalloc
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
	asm volatile("sti");

	// Stops my CPU form running away
	for(;;)
	{
		asm volatile("hlt");
	}
}
