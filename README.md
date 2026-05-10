# GordOS

A hobby OS built from scratch in C and x86 Assembly, made to learn the fundamentals of OS development. GordOS is not intended for real use. It exists purely as a learning project.

> **Disclaimer:** I am not a professional. I am not responsible for any damage caused to your system. **Use at your own risk.**

---

## Milestone Reached: Higher-Half Kernel

GordOS now runs as a proper higher-half kernel, with the kernel mapped at virtual `0xC0000000` while low physical memory remains identity-mapped for hardware access. This is the foundation for user mode and process isolation down the line.

---

## What GordOS Can Do

- Boots via GRUB on real x86 hardware and QEMU
- VGA text mode terminal with colour, scrolling, and a hardware cursor
- PS/2 keyboard driver with full scancode translation, shift, and arrow keys
- Interactive shell with command history and mid-line cursor movement
- Tab autocomplete for both commands and filenames/directories
- Physical memory manager with bitmap allocator
- Kernel heap allocator (kmalloc/kfree) with splitting and coalescing
- ATA PIO disk driver
- FAT32 filesystem: mount, list directories, read files, create, write, delete, and rename files, directory creation and navigation
- Higher-half kernel at virtual `0xC0000000` with identity-mapped low memory
- Page fault handler that prints the faulting address from CR2 and error code
- PIT driver at 1000Hz (timer_ticks, timer_sleep)
- RTC driver reading real wall-clock time from CMOS
- Shell commands: `help`, `clear`, `echo`, `about`, `ls`, `pwd`, `cat`, `touch`, `write`, `rm`, `rename`, `mkdir`, `cd`, `time`

---

## Development Status

Active development.

**Upcoming work (roughly in order):**

**Near term**
- Subdirectory navigation in `cat`, `write`, `rm`, `rename` (currently only works in cwd)
- FAT32 long filename (LFN) support
- File overwrite improvements

**Medium term**
- Syscall interface (int 0x80)
- User mode (ring 3)
- Basic process/task structure
- Round-robin scheduler

**Long term**
- ELF executable loading
- Proper VFS layer abstracting FAT32 behind a unified file interface
- Serial port driver (useful for real hardware debugging)
- More shell built-ins as the OS grows

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
| Stack | in .bss | higher half, 16 KB |
| Heap | dynamic | managed by kmalloc |

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
make disk   # Create a fresh FAT32 disk.img for QEMU
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

- 16KB stack — fine for now, will need addressing before user mode
- Filenames must be 8.3 uppercase format (e.g. `TEST.TXT`) — input can be lowercase
- No subdirectory support in `cat`, `write`, `rm`, `rename` — all commands operate on the current working directory only
- Filenames written via `write` command may have their extension truncated by one character in some cases (e.g. `TEST.TXT` becomes `TEST.TX`)