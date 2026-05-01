# GordOS

A hobby OS built from scratch in C and x86 Assembly, made to learn the fundamentals of OS development. GordOS is not intended for real use. It exists purely as a learning project.

> **Disclaimer:** I am not a professional. I am not responsible for any damage caused to your system. **Use at your own risk.**

---

## 🎉 Milestone Reached: Full FAT32 Read/Write Support

GordOS can now create, write, and read files entirely from within the OS itself, no Linux middleman required. Type `write MYFILE.TXT Hello world` and `cat MYFILE.TXT` and it just works. Built from scratch. On bare metal. In C and Assembly.

Not bad for an OS that sucks (for now :3).

---

## What GordOS Can Do

- Boots via GRUB on real x86 hardware and QEMU
- VGA text mode terminal with colour, scrolling, and a hardware cursor
- PS/2 keyboard driver with full scancode translation, shift, and arrow keys
- Interactive shell with command history and mid-line cursor movement
- Physical memory manager with bitmap allocator
- Kernel heap allocator (kmalloc/kfree)
- ATA PIO disk driver
- FAT32 filesystem: mount, list directories, read files, **create and write files**
- Shell commands: `help`, `clear`, `echo`, `about`, `ls`, `cat`, `touch`, `write`

---

## Development Status

Active development. The filesystem is now fully functional for basic operations.

**Upcoming work:**
- Subdirectory navigation
- Virtual memory and paging
- File overwrite support (currently creates a new entry if file exists)

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
| `-fno-stack-protector` | Disable stack smashing protection |

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
make iso    # Create the bootable ISO
make clean  # Remove all build artifacts
```

---

## Running GordOS

### QEMU (recommended)

Create a FAT32 disk image:

```bash
qemu-img create -f raw disk.img 64M
mkfs.fat -F 32 disk.img
```

To put files on the disk from Linux:

```bash
echo "Hello from GordOS!" > test.txt
mcopy -i disk.img test.txt ::TEST.TXT
```

Then boot:

```bash
qemu-system-i386 -cdrom GordOS.iso -drive file=disk.img,format=raw -boot d
```

### Other Options

- **USB drive** — `dd` the ISO to a USB device
- **Hard disk** — `dd` the ISO to a disk
- **Optical media** — burn the ISO to a DVD or CD

---

## Known Issues

- 16KB stack — fine for now but will need addressing in the future
- No virtual memory or paging — kernel runs in a flat physical memory model
- Filenames must be uppercase 8.3 format (e.g. `TEST.TXT`) due to FAT32 limitations
- No subdirectory support yet — all commands only work in the root directory
- Writing to an existing file creates a duplicate entry rather than overwriting
