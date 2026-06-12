# GordOS

A hobby OS built from scratch in C and x86 Assembly, made to learn the fundamentals of OS development. GordOS is not intended for real use. It exists purely as a learning project.

> **Disclaimer:** I am not a professional. I am not responsible for any damage caused to your system. **Use at your own risk.**

---

## Milestone Reached: Running User Programs From Disk

GordOS can now load an ELF executable from the FAT32 disk and run it as a real user process: the `exec` shell command reads the binary, creates a process with its own page directory and kernel stack, loads the `PT_LOAD` segments, maps a user stack, and drops to ring 3. When the program calls `sys_exit`, control returns cleanly to the shell. A sample user program lives in `user/hello.c` and is copied onto the disk image by `make disk` — try `exec HELLO.ELF`.

---

## What GordOS Can Do

- Boots via GRUB on real x86 hardware and QEMU
- Higher-half kernel running at virtual `0xC0000000`
- VGA text mode terminal with colour, scrolling, and a hardware cursor
- PS/2 keyboard driver with full scancode translation, shift, and arrow keys
- Interactive shell with command history, mid-line cursor movement, and tab autocomplete
- Tab autocomplete for both commands and filenames/directories (including subdirectories)
- Physical memory manager with bitmap allocator
- Kernel heap allocator (kmalloc/kfree) with splitting and coalescing
- ATA PIO disk driver
- FAT32 filesystem: mount, list, read, create, write, delete, rename files and directories
- Subdirectory navigation with absolute and relative path support
- Page fault handler that prints the faulting address and error code
- PIT driver at 1000Hz (timer_ticks, timer_sleep)
- RTC driver reading real wall-clock time from CMOS
- Syscall interface via `int 0x80` (write, exit, getpid, read, sleep) with return values in `eax`
- Faulting user processes are killed and control returns to the shell
- Ring 3 GDT segments, TSS, and `jump_to_usermode`
- `paging_map_page` with user bit support for mapping user-accessible pages
- Process structures with per-process page directories and kernel stacks
- Round-robin scheduler scaffolding (context switch, ready queue, PIT-driven preemption)
- ELF executable loader (PT_LOAD segments into a process address space)
- `exec` runs ELF binaries from disk in ring 3 and returns to the shell when they exit
- Kernel heap and VGA mappings live in the higher half, valid in every address space
- Sample user programs (`user/hello.c`, interactive `user/echo.c`) built by `make user`, installed by `make disk`
- Shell commands: `help`, `clear`, `echo`, `about`, `ls [path]`, `pwd`, `cat`, `touch`, `write`, `rm`, `rename`, `mkdir`, `cd`, `exec`, `time`, `uptime`, `free`

---

## Development Status

Active development.

**Upcoming work (roughly in order):**

**Near term**
- Background processes — wire the round-robin scheduler so multiple
  processes can run concurrently instead of `exec` blocking the shell
- File syscalls (open/read/write) so user programs can use the filesystem

**Medium term**
- VFS layer abstracting FAT32 behind a unified file interface
- Serial port driver (useful for real hardware debugging)
- FAT32 long filename (LFN) support

**Long term**
- More shell built-ins as the OS grows
- Networking (very ambitious)

---

## Technical Specifications

### Build Requirements

| Item | Detail |
| :--- | :--- |
| Architecture | i686 (32-bit) |
| Compiler | `i686-elf-gcc` ([OSDev cross-compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler)) |
| Assembler | `i686-elf-as` (covered in the same guide) |
| Optimization | `-O2` |
| ISO tool | `xorriso` |
| Disk tool | `mtools` |

### Hardware Requirements

| Component | Requirement |
| :--- | :--- |
| CPU | 32-bit i686 |
| RAM | 725 MB minimum (tested, could be lower) |
| Video | VGA text mode |
| Firmware | BIOS / Legacy boot |
| Input | PS/2 keyboard (Scan Code Set 1) |
| Storage | ATA disk (required for filesystem) |

### Memory Layout

| Region | Physical | Virtual |
| :--- | :--- | :--- |
| Boot code + multiboot header | `0x00200000` | `0x00200000` |
| Kernel text/rodata/data/bss | `0x00202000+` | `0xC0202000+` |
| Identity map (first 4MB) | `0x00000000` | `0x00000000` |
| Stack | in .bss | higher half, 64 KB |
| Heap | dynamic | managed by kmalloc |
| User code | `0x00100000` | `0x00100000` |
| User stack | `0x00xxxxx` | `0xBFFFF000` |

### Syscall Convention

| Register | Purpose |
| :--- | :--- |
| `eax` | Syscall number |
| `ebx` | Argument 1 |
| `ecx` | Argument 2 |
| `edx` | Argument 3 |
| `eax` (return) | Return value |

| Number | Name | Description |
| :--- | :--- | :--- |
| 0 | `sys_write` | Write buffer to terminal |
| 1 | `sys_exit` | Terminate process, return to shell |
| 2 | `sys_getpid` | Get current process ID |
| 3 | `sys_read` | Read keyboard input (blocks until at least 1 byte) |
| 4 | `sys_sleep` | Sleep for N milliseconds |

### Compilation Flags

| Flag | Purpose |
| :--- | :--- |
| `-ffreestanding` | Build without the standard library |
| `-nostdlib` | Prevent linking standard C startup files |
| `-O2` | Optimisation level 2 |

---

## Building GordOS

### 1. Install Dependencies

```bash
sudo apt install xorriso mtools grub-pc-bin grub-common
```

### 2. Clone the Repository

```bash
git clone https://github.com/Millenium2133/GordOS.git
cd GordOS
```

### 3. Set Up the Cross-Compiler

Follow the [OSDev cross-compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler), then add to your `~/.bashrc`:

```bash
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
```

Verify the compiler is ready:

```bash
i686-elf-gcc --version
# Expected output: i686-elf-gcc (GCC) x.x.x
```

### 4. Build and Create the ISO

```bash
make        # Compile and link everything
make iso    # Create the bootable ISO
make user   # Build the sample user program (user/hello.elf)
make disk   # Create a fresh FAT32 disk.img with HELLO.ELF installed
make clean  # Remove all build artifacts
```

---

## Running GordOS

### QEMU (recommended)

```bash
make disk   # Only needed once, or after make clean
make iso
make run
```

To put files on the disk from Linux before booting:

```bash
mcopy -i disk.img test.txt ::TEST.TXT
```

You can also create and write files from within GordOS using the built-in shell commands.

### Other Options

- **USB drive** — `dd` the ISO to a USB device
- **Hard disk** — `dd` the ISO to a disk
- **Optical media** — burn the ISO to a DVD or CD

### Real x86 Hardware

- Build the ISO and `dd` it to a USB drive
- Format a hard drive or partition as FAT32 for filesystem support
- Boot from USB — should work out of the box on any BIOS/legacy boot system

## Notes on Real Hardware

- **Less stable** — things may work in QEMU before they're fixed for real hardware
- **Don't remove the USB while running**
- **Data loss risk** — formatting a partition as FAT32 will erase its contents

> **I AM NOT RESPONSIBLE FOR ANY DATA LOSS. YOU HAVE BEEN WARNED.**

---

## Known Issues

- Filenames must be 8.3 uppercase format (e.g. `TEST.TXT`), input can be lowercase
- Shell currently runs in ring 0, will be moved to user mode when process support is complete
- Kernel heap is limited to the identity-mapped low 4MB of physical memory