# GordOS

A hobby OS built to learn the fundamentals of OS development. GordOS is not intended for real use, it exists purely as a learning project.

> **Disclaimer:** I am not a professional. I am not responsible for any damage caused to your system. **Use at your own risk.**

---

## Development Status

Development is ongoing but at a reduced pace while I focus on studies and other projects.

**Completed:**
- Multiboot bootloader via GRUB
- GDT and IDT setup
- 8259 PIC remapping and IRQ handling
- VGA text mode terminal with scrolling and colour support
- PS/2 keyboard driver with full scancode translation
- Interactive shell with command history and arrow key navigation
- Physical memory manager with bitmap allocator
- Kernel heap allocator (kmalloc/kfree)
- ATA PIO disk driver

**Upcoming work:**
- FAT32 filesystem
- Shell commands that interact with the filesystem (ls, touch, cat, etc.)

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
| Display | VGA Text Mode (`0xB8000`) |
| Storage | ATA disk (for filesystem, optional) |

### Memory Layout

| Section | Start Address | Size |
| :--- | :--- | :--- |
| Multiboot | `0x00100000` | 12 bytes |
| Kernel Text | `0x0010000C` | ~[variable] |
| Stack | Defined in `boot.s` | 16 KB |
| Heap | Dynamic | Managed by kmalloc |

### Compilation Flags

| Flag | Purpose |
| :--- | :--- |
| `-ffreestanding` | Build without the standard library |
| `-nostdlib` | Prevent linking standard C startup files |
| `-fno-stack-protector` | Disable stack smashing protection (requires kernel support) |

---

## Building GordOS

### 1. Install Dependencies

```bash
sudo apt install xorriso mtools grub-pc-bin grub-common
```

### 2. Clone the Repository

```bash
git clone https://github.com/Millenium2133/GordOS.git
sudo chmod -R $USER:$USER GordOS/
cd GordOS
```

### 3. Set Up the Cross-Compiler

```bash
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
export PATH="$HOME/opt/cross/bin:$PATH"
```

Verify the compiler is ready:

```bash
$TARGET-gcc --version
# Expected output: i686-elf-gcc (GCC) x.x.x
```

### 4. Build and Create the ISO

> **Note:** `make` may error on the first run. If it does, simply run it again.

```bash
make        # Compile and link everything
make iso    # Create the ISO file (output to current directory)
make clean  # Remove all build artifacts
```

You will now have an ISO file ready to use.

---

## Running GordOS

### QEMU (recommended for testing)

First create a disk image for ATA storage:

```bash
qemu-img create -f raw disk.img 64M
```

Then run:

```bash
qemu-system-i386 -cdrom GordOS.iso -drive file=disk.img,format=raw
```

### Other options

- **USB drive** — `dd` the ISO to a USB device
- **Hard disk** — `dd` the ISO to a disk
- **Optical media** — burn the ISO to a DVD or CD

---

## Known Issues

- 16KB stack, which is fine for now but will need to be addressed in the future
- No virtual memory or paging — kernel runs in a flat physical memory model
- Backspace and arrow keys have some edge case bugs in the shell

## What's Next

- **FAT32 Filesystem**
- **Filesystem shell commands** — ls, touch, cat, mkdir
- **Virtual Memory Manager**
