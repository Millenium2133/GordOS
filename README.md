# GordOS

A hobby OS built to learn the fundamentals of OS development. GordOS is not intended for real use — it exists purely as a learning project.

> **Disclaimer:** I am not a professional. I am not responsible for any damage caused to your system. **Use at your own risk.**

---

## Development Status

Development is ongoing but at a reduced pace while I focus on studies and other projects.

**Upcoming work:**
- Memory management
- Filesystem (likely FAT32)

---

## Technical Specifications

### Build Requirements

| Item | Detail |
| :--- | :--- |
| Architecture | i686 (32-bit) |
| Compiler | `i686-elf-gcc` ([OSDev cross-compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler)) |
| Assembler | `i686-elf-as` (covered in the same guide) |
| Optimization | `-O2` |

### Hardware Requirements

| Component | Requirement |
| :--- | :--- |
| CPU | 32-bit i686 |
| RAM | 725 MB minimum (tested) |
| Video | VGA text mode |
| Firmware | BIOS / Legacy boot |
| Input | PS/2 keyboard (Scan Code Set 1) |
| Display | VGA Text Mode (`0xB8000`) |

### Memory Layout

| Section | Start Address | Size |
| :--- | :--- | :--- |
| Multiboot | `0x00100000` | 12 bytes |
| Kernel Text | `0x0010000C` | ~[variable] |
| Stack | Defined in `boot.s` | 16 KB |

### Compilation Flags

| Flag | Purpose |
| :--- | :--- |
| `-ffreestanding` | Build without the standard library |
| `-nostdlib` | Prevent linking standard C startup files |
| `-fno-stack-protector` | Disable stack smashing protection (requires kernel support) |

---

## Building GordOS

### 1. Clone the Repository

```bash
git clone https://github.com/Millenium2133/GordOS.git
sudo chmod -R 777 GordOS/
cd GordOS
```

### 2. Set Up the Cross-Compiler

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

### 3. Build and Create the ISO

> **Note:** `make` may error on the first run. If it does, simply run it again.

```bash
make        # Compile and link everything
make iso    # Create the ISO file (output to current directory)
make clean  # Remove all build artifacts
```

You will now have an ISO file ready to use.

---

## Running GordOS

You can run GordOS in a few different ways:

- **QEMU** — the easiest option for testing
- **USB drive** — `dd` the ISO to a USB device
- **Hard disk** — `dd` the ISO to a disk
- **Optical media** — burn the ISO to a DVD or CD
