CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-gcc

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -Icpu -Idrivers -Idisplay -Ilib -Ikernel \
	-Iboot -Imemory

OBJS = boot.o kernel.o gdt.o gdt_flush.o idt.o idt_flush.o \
       isr.o pic.o keyboard.o splash.o string.o vga.o shell.o pmm.o \
	kmalloc.o

GordOS: $(OBJS) boot/linker.ld
	$(LD) -T boot/linker.ld -o GordOS -ffreestanding -O2 -nostdlib $(OBJS) -lgcc

boot.o: boot/boot.s
	$(AS) boot/boot.s -o boot.o
pmm.o: memory/pmm.c memory/pmm.h boot/multiboot.h
	$(CC) $(CFLAGS) -c memory/pmm.c -o pmm.o

kmalloc.o: memory/kmalloc.c memory/kmalloc.h memory/pmm.h
	$(CC) $(CFLAGS) -c memory/kmalloc.c -o kmalloc.o

shell.o: kernel/shell.c kernel/shell.h display/vga.h lib/string.h
	$(CC) $(CFLAGS) -c kernel/shell.c -o shell.o

kernel.o: kernel/kernel.c cpu/gdt.h cpu/idt.h drivers/pic.h drivers/keyboard.h display/vga.h display/splash.h lib/string.h kernel/shell.h
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel.o

gdt.o: cpu/gdt.c cpu/gdt.h
	$(CC) $(CFLAGS) -c cpu/gdt.c -o gdt.o

gdt_flush.o: cpu/gdt_flush.s
	$(AS) cpu/gdt_flush.s -o gdt_flush.o

idt.o: cpu/idt.c cpu/idt.h drivers/pic.h
	$(CC) $(CFLAGS) -c cpu/idt.c -o idt.o

idt_flush.o: cpu/idt_flush.s
	$(AS) cpu/idt_flush.s -o idt_flush.o

isr.o: cpu/isr.s
	$(AS) cpu/isr.s -o isr.o

pic.o: drivers/pic.c drivers/pic.h
	$(CC) $(CFLAGS) -c drivers/pic.c -o pic.o

keyboard.o: drivers/keyboard.c drivers/keyboard.h cpu/idt.h drivers/pic.h kernel/shell.h
	$(CC) $(CFLAGS) -c drivers/keyboard.c -o keyboard.o

vga.o: display/vga.c display/vga.h lib/string.h drivers/pic.h
	$(CC) $(CFLAGS) -c display/vga.c -o vga.o

splash.o: display/splash.c display/splash.h display/vga.h
	$(CC) $(CFLAGS) -c display/splash.c -o splash.o

string.o: lib/string.c lib/string.h
	$(CC) $(CFLAGS) -c lib/string.c -o string.o

iso: GordOS
	mkdir -p isodir/boot/grub
	cp GordOS isodir/boot/GordOS
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o GordOS.iso isodir

clean:
	rm -rf *.o GordOS GordOS.iso
	rm -rf isodir/

.PHONY: clean iso
