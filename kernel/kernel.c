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


// Compiler check
#if defined(__linux__)
#error "You are not using a cross compiler"
#endif

#if !defined(__i386__)
#error "This kernel needs to be compiled with an ix86-elf compiler"
#endif

void kernel_main(void)
{
	gdt_init();
	idt_init();
	pic_remap();
	terminal_initialize();

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
