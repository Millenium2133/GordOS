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


static uint8_t user_code_buf[4096]  __attribute__((aligned(4096)));
static uint8_t user_stack_buf[4096] __attribute__((aligned(4096)));

static void launch_user_process(void)
{
	uint32_t code_phys  = (uint32_t)user_code_buf  - 0xC0000000;
	uint32_t stack_phys = (uint32_t)user_stack_buf - 0xC0000000;

	paging_map_page(0x00100000, code_phys,  PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER);
	paging_map_page(0xBFFFF000, stack_phys, PAGE_PRESENT | PAGE_WRITEABLE | PAGE_USER);

	uint8_t* code = user_code_buf;

	// mov eax, 0  (SYS_WRITE)
	code[0]  = 0xB8; code[1]  = 0x00; code[2]  = 0x00; code[3]  = 0x00; code[4]  = 0x00;
	// mov ebx, 0x00100020  (message pointer)
	code[5]  = 0xBB; code[6]  = 0x20; code[7]  = 0x00; code[8]  = 0x10; code[9]  = 0x00;
	// mov ecx, 19  (length)
	code[10] = 0xB9; code[11] = 0x13; code[12] = 0x00; code[13] = 0x00; code[14] = 0x00;
	// int 0x80
	code[15] = 0xCD; code[16] = 0x80;
	// mov eax, 1  (SYS_EXIT)
	code[17] = 0xB8; code[18] = 0x01; code[19] = 0x00; code[20] = 0x00; code[21] = 0x00;
	// mov ebx, 0
	code[22] = 0xBB; code[23] = 0x00; code[24] = 0x00; code[25] = 0x00; code[26] = 0x00;
	// int 0x80
	code[27] = 0xCD; code[28] = 0x80;
	// hlt  (safety net)
	code[29] = 0xF4;

	// Write message at offset 0x20 in the code page
	const char* msg = "Hello from ring 3!\n";
	uint8_t* msg_dest = user_code_buf + 0x20;
	int i;
	for (i = 0; msg[i]; i++)
		msg_dest[i] = (uint8_t)msg[i];

	uint32_t user_esp = 0xBFFFFFFC;
	uint32_t user_eip = 0x00100000;

	terminal_writestring("Launching user process...\n");
	jump_to_usermode(user_eip, user_esp);
}

void kernel_main(uint32_t magic, multiboot_info_t* mbi)
{
	terminal_initialize();
	(void)magic;
	gdt_init();
	pic_remap();
	idt_init();
	syscall_init();
	pmm_init(mbi);
	paging_init();
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
	asm volatile("sti");
	

	for (;;)
		asm volatile("hlt");
}