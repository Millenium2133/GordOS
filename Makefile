CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-gcc

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra

OBJS = boot.o kernel.o gdt.o gdt_flush.o

myos: $(OBJS) linker.ld
	$(LD) -T linker.ld -o myos -ffreestanding -O2 -nostdlib $(OBJS) -lgcc

gdt_flush.o: gdt_flush.s
	$(AS) gdt_flush.s -o gdt_flush.o

gdt.o: gdt.c gdt.h
	$(CC) $(CFLAGS) -c gdt.c -o gdt.o

boot.o: boot.s
	$(AS) boot.s -o boot.o

kernel.o: kernel.c gdt.h
	$(CC) $(CFLAGS) -c kernel.c -o kernel.o

iso: myos
	mkdir -p isodir/boot/grub
	cp myos isodir/boot/myos
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub2-mkrescue -o myos.iso isodir

clean:
	rm -rf *.o myos myos.iso
	rm -rf isodir/

.PHONY: clean iso
