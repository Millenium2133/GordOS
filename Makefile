CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-gcc

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -Icpu -Idrivers -Idisplay -Ilib -Ikernel \
	-Iboot -Imemory -Ifs

OBJS = boot.o kernel.o gdt.o gdt_flush.o idt.o idt_flush.o \
       isr.o pic.o keyboard.o splash.o string.o vga.o shell.o pmm.o \
	kmalloc.o ata.o fat32.o paging.o pit.o rtc.o syscall.o \
	usermode.o process.o elf.o scheduler.o scheduler_asm.o

GordOS: $(OBJS) boot/linker.ld
	$(LD) -T boot/linker.ld -o GordOS -ffreestanding -O2 -nostdlib $(OBJS) -lgcc

boot.o: boot/boot.s
	$(AS) boot/boot.s -o boot.o

scheduler.o: kernel/scheduler.c kernel/scheduler.h kernel/process.h memory/paging.h
	$(CC) $(CFLAGS) -c kernel/scheduler.c -o scheduler.o

scheduler_asm.o: kernel/scheduler.s
	$(AS) kernel/scheduler.s -o scheduler_asm.o

pit.o: drivers/pit.c drivers/pit.h drivers/pic.h cpu/idt.h
	$(CC) $(CFLAGS) -c drivers/pit.c -o pit.o

rtc.o: drivers/rtc.c drivers/rtc.h drivers/pic.h
	$(CC) $(CFLAGS) -c drivers/rtc.c -o rtc.o

pmm.o: memory/pmm.c memory/pmm.h boot/multiboot.h
	$(CC) $(CFLAGS) -c memory/pmm.c -o pmm.o

paging.o: memory/paging.c memory/paging.h memory/pmm.h
	$(CC) $(CFLAGS) -c memory/paging.c -o paging.o

kmalloc.o: memory/kmalloc.c memory/kmalloc.h memory/pmm.h
	$(CC) $(CFLAGS) -c memory/kmalloc.c -o kmalloc.o

ata.o: drivers/ata.c drivers/ata.h drivers/pic.h
	$(CC) $(CFLAGS) -c drivers/ata.c -o ata.o

fat32.o: fs/fat32.c fs/fat32.h drivers/ata.h memory/kmalloc.h
	$(CC) $(CFLAGS) -Ifs -c fs/fat32.c -o fat32.o

shell.o: kernel/shell.c kernel/shell.h display/vga.h lib/string.h drivers/rtc.h
	$(CC) $(CFLAGS) -c kernel/shell.c -o shell.o

process.o: kernel/process.c kernel/process.h memory/paging.h memory/kmalloc.h
	$(CC) $(CFLAGS) -c kernel/process.c -o process.o

elf.o: kernel/elf.c kernel/elf.h kernel/process.h memory/paging.h memory/pmm.h
	$(CC) $(CFLAGS) -c kernel/elf.c -o elf.o

usermode.o: kernel/usermode.c kernel/usermode.h cpu/idt.h
	$(CC) $(CFLAGS) -c kernel/usermode.c -o usermode.o

syscall.o: kernel/syscall.c kernel/syscall.h cpu/idt.h
	$(CC) $(CFLAGS) -c kernel/syscall.c -o syscall.o

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
	rm -rf disk.img
disk:
	qemu-img create -f raw disk.img 64M
	mkfs.fat -F 32 disk.img

run: GordOS.iso
	@test -f disk.img || (echo "ERROR: disk.img not found, run 'make disk' first" && exit 1)
	qemu-system-i386 -boot order=d -rtc base=localtime -cdrom GordOS.iso -drive file=disk.img,format=raw,if=ide,index=0

.PHONY: clean iso run
