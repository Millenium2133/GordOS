CC = i686-elf-gcc
AS = i686-elf-as
LD = i686-elf-gcc

CFLAGS = -std=gnu99 -ffreestanding -O2 -Wall -Wextra \
         -Icpu -Idrivers -Idisplay -Ilib -Ikernel -Iboot -Imemory -Ifs

# +------------------+
# + Object Files     +
# +------------------+

OBJS = \
	boot.o \
	\
	cpu/gdt.o \
	cpu/gdt_flush.o \
	cpu/idt.o \
	cpu/idt_flush.o \
	cpu/isr.o \
	\
	drivers/pic.o \
	drivers/keyboard.o \
	drivers/ata.o \
	drivers/pit.o \
	drivers/rtc.o \
	drivers/serial.o \
	\
	display/vga.o \
	display/splash.o \
	\
	lib/string.o \
	\
	memory/pmm.o \
	memory/paging.o \
	memory/kmalloc.o \
	\
	fs/fat32.o \
	\
	kernel/kernel.o \
	kernel/shell.o \
	kernel/syscall.o \
	kernel/usermode.o \
	kernel/usermode_asm.o \
	kernel/process.o \
	kernel/scheduler.o \
	kernel/scheduler_asm.o \
	kernel/elf.o \
	kernel/wbuf.o

# +------------------+
# + Primary Targets  +
# +------------------+

GordOS: $(OBJS) boot/linker.ld
	$(LD) -T boot/linker.ld -o GordOS -ffreestanding -O2 -nostdlib $(OBJS) -lgcc

iso: GordOS
	mkdir -p isodir/boot/grub
	cp GordOS isodir/boot/GordOS
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o GordOS.iso isodir

disk: user
	qemu-img create -f raw disk.img 64M
	mkfs.fat -F 32 disk.img
	mcopy -i disk.img user/hello.elf ::HELLO.ELF
	mcopy -i disk.img user/echo.elf ::ECHO.ELF
	mcopy -i disk.img user/files.elf ::FILES.ELF
	mcopy -i disk.img user/crash.elf ::CRASH.ELF
	mcopy -i disk.img user/counter.elf ::COUNTER.ELF
	mcopy -i disk.img user/forktest.elf ::FORKTEST.ELF
	mcopy -i disk.img user/fdcat.elf ::FDCAT.ELF

run: GordOS.iso
	@test -f disk.img || (echo "ERROR: disk.img not found, run 'make disk' first" && exit 1)
	qemu-system-i386 -boot order=d -rtc base=localtime \
		-serial stdio \
		-cdrom GordOS.iso \
		-drive file=disk.img,format=raw,if=ide,index=0

clean:
	rm -rf *.o GordOS GordOS.iso isodir/ disk.img
	rm -rf cpu/*.o drivers/*.o display/*.o lib/*.o memory/*.o fs/*.o kernel/*.o
	rm -rf user/*.elf

.PHONY: clean iso disk run user

# +------------------+
# + Boot             +
# +------------------+

boot.o: boot/boot.s
	$(AS) boot/boot.s -o boot.o

# +------------------+
# + CPU              +
# +------------------+

cpu/gdt.o: cpu/gdt.c cpu/gdt.h
	$(CC) $(CFLAGS) -c cpu/gdt.c -o cpu/gdt.o

cpu/gdt_flush.o: cpu/gdt_flush.s
	$(AS) cpu/gdt_flush.s -o cpu/gdt_flush.o

cpu/idt.o: cpu/idt.c cpu/idt.h drivers/pic.h kernel/process.h
	$(CC) $(CFLAGS) -c cpu/idt.c -o cpu/idt.o

cpu/idt_flush.o: cpu/idt_flush.s
	$(AS) cpu/idt_flush.s -o cpu/idt_flush.o

cpu/isr.o: cpu/isr.s
	$(AS) cpu/isr.s -o cpu/isr.o

# +------------------+
# + Drivers          +
# +------------------+

drivers/pic.o: drivers/pic.c drivers/pic.h
	$(CC) $(CFLAGS) -c drivers/pic.c -o drivers/pic.o

drivers/keyboard.o: drivers/keyboard.c drivers/keyboard.h cpu/idt.h drivers/pic.h \
                    kernel/shell.h kernel/process.h
	$(CC) $(CFLAGS) -c drivers/keyboard.c -o drivers/keyboard.o

drivers/ata.o: drivers/ata.c drivers/ata.h drivers/pic.h
	$(CC) $(CFLAGS) -c drivers/ata.c -o drivers/ata.o

drivers/pit.o: drivers/pit.c drivers/pit.h drivers/pic.h cpu/idt.h kernel/scheduler.h
	$(CC) $(CFLAGS) -c drivers/pit.c -o drivers/pit.o

drivers/rtc.o: drivers/rtc.c drivers/rtc.h drivers/pic.h
	$(CC) $(CFLAGS) -c drivers/rtc.c -o drivers/rtc.o

drivers/serial.o: drivers/serial.c drivers/serial.h drivers/pic.h
	$(CC) $(CFLAGS) -c drivers/serial.c -o drivers/serial.o

# +------------------+
# + Display          +
# +------------------+

display/vga.o: display/vga.c display/vga.h lib/string.h drivers/pic.h drivers/serial.h
	$(CC) $(CFLAGS) -c display/vga.c -o display/vga.o

display/splash.o: display/splash.c display/splash.h display/vga.h
	$(CC) $(CFLAGS) -c display/splash.c -o display/splash.o

# +------------------+
# + Lib              +
# +------------------+

lib/string.o: lib/string.c lib/string.h
	$(CC) $(CFLAGS) -c lib/string.c -o lib/string.o

# +------------------+
# + Memory           +
# +------------------+

memory/pmm.o: memory/pmm.c memory/pmm.h boot/multiboot.h
	$(CC) $(CFLAGS) -c memory/pmm.c -o memory/pmm.o

memory/paging.o: memory/paging.c memory/paging.h memory/pmm.h
	$(CC) $(CFLAGS) -c memory/paging.c -o memory/paging.o

memory/kmalloc.o: memory/kmalloc.c memory/kmalloc.h memory/pmm.h
	$(CC) $(CFLAGS) -c memory/kmalloc.c -o memory/kmalloc.o

# +------------------+
# + Filesystem       +
# +------------------+

fs/fat32.o: fs/fat32.c fs/fat32.h drivers/ata.h memory/kmalloc.h
	$(CC) $(CFLAGS) -Ifs -c fs/fat32.c -o fs/fat32.o

# +------------------+
# + Kernel           +
# +------------------+

kernel/kernel.o: kernel/kernel.c cpu/gdt.h cpu/idt.h drivers/pic.h \
                 drivers/keyboard.h display/vga.h display/splash.h \
                 lib/string.h kernel/shell.h kernel/process.h \
                 kernel/scheduler.h kernel/elf.h
	$(CC) $(CFLAGS) -c kernel/kernel.c -o kernel/kernel.o

kernel/shell.o: kernel/shell.c kernel/shell.h display/vga.h lib/string.h \
                drivers/rtc.h drivers/pit.h fs/fat32.h memory/pmm.h memory/kmalloc.h \
                kernel/process.h kernel/elf.h memory/paging.h drivers/keyboard.h
	$(CC) $(CFLAGS) -c kernel/shell.c -o kernel/shell.o

kernel/syscall.o: kernel/syscall.c kernel/syscall.h cpu/idt.h kernel/process.h \
                  drivers/keyboard.h drivers/pit.h fs/fat32.h
	$(CC) $(CFLAGS) -c kernel/syscall.c -o kernel/syscall.o

kernel/usermode.o: kernel/usermode.c kernel/usermode.h cpu/gdt.h
	$(CC) $(CFLAGS) -c kernel/usermode.c -o kernel/usermode.o

kernel/usermode_asm.o: kernel/usermode.s
	$(AS) kernel/usermode.s -o kernel/usermode_asm.o

kernel/process.o: kernel/process.c kernel/process.h memory/paging.h memory/kmalloc.h cpu/gdt.h
	$(CC) $(CFLAGS) -c kernel/process.c -o kernel/process.o

kernel/scheduler.o: kernel/scheduler.c kernel/scheduler.h kernel/process.h memory/paging.h cpu/gdt.h
	$(CC) $(CFLAGS) -c kernel/scheduler.c -o kernel/scheduler.o

kernel/scheduler_asm.o: kernel/scheduler.s
	$(AS) kernel/scheduler.s -o kernel/scheduler_asm.o

kernel/elf.o: kernel/elf.c kernel/elf.h kernel/process.h memory/paging.h memory/pmm.h lib/string.h
	$(CC) $(CFLAGS) -c kernel/elf.c -o kernel/elf.o

kernel/wbuf.o: kernel/wbuf.c kernel/wbuf.h fs/fat32.h memory/kmalloc.h lib/string.h
	$(CC) $(CFLAGS) -c kernel/wbuf.c -o kernel/wbuf.o

# +------------------+
# + User Programs    +
# +------------------+

user: user/hello.elf user/echo.elf user/files.elf user/crash.elf user/counter.elf user/forktest.elf user/fdcat.elf

user/hello.elf: user/hello.c user/linker.ld
	$(CC) -std=gnu99 -ffreestanding -O2 -Wall -Wextra -nostdlib \
	      -T user/linker.ld user/hello.c -o user/hello.elf

user/echo.elf: user/echo.c user/linker.ld
	$(CC) -std=gnu99 -ffreestanding -O2 -Wall -Wextra -nostdlib \
	      -T user/linker.ld user/echo.c -o user/echo.elf

user/files.elf: user/files.c user/linker.ld
	$(CC) -std=gnu99 -ffreestanding -O2 -Wall -Wextra -nostdlib \
	      -T user/linker.ld user/files.c -o user/files.elf

user/crash.elf: user/crash.c user/linker.ld
	$(CC) -std=gnu99 -ffreestanding -O2 -Wall -Wextra -nostdlib \
	      -T user/linker.ld user/crash.c -o user/crash.elf

user/counter.elf: user/counter.c user/linker.ld
	$(CC) -std=gnu99 -ffreestanding -O2 -Wall -Wextra -nostdlib \
	      -T user/linker.ld user/counter.c -o user/counter.elf

user/forktest.elf: user/forktest.c user/linker.ld
	$(CC) -std=gnu99 -ffreestanding -O2 -Wall -Wextra -nostdlib \
	      -T user/linker.ld user/forktest.c -o user/forktest.elf

user/fdcat.elf: user/fdcat.c user/linker.ld
	$(CC) -std=gnu99 -ffreestanding -O2 -Wall -Wextra -nostdlib \
	      -T user/linker.ld user/fdcat.c -o user/fdcat.elf