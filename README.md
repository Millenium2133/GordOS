# GordOS

## Description
GordOS is a hobby OS I am making to help me get a better understanding of OSDev. This by no means is a "Real OS", It's especially not one you should use (even if you could). THIS IS JUST A HOBBY PROJECT, I BY NO MEANS AM A PROFESSIONAL. I AM NOT RESPONSABLE FOR ANY DAMAGE CAUSED TO YOUR SYSTEMS. USE AT YOUR OWN RISK

---

## Technical Specifications

### Build Requirements
* **Architecture:** i686 (32-bit)
* **Compiler:** i686-elf-gcc (I built mine from the OSDev gcc cross-compiler wiki, i reccomend you do the same)
* **Assembler:** i686-elf-as (coverd in the OSDev gcc cross compiler wiki)
* **Optimization Level:** O2

### Hardware Environment
* **Platform:** x86 (IBM PC Compatible)
* **Display:** VGA Text Mode (0xB8000)
* **Input:** PS/2 Keyboard (Scan Code Set 1)

### Hardware Requirements
* **RAM:** The least I've been able to test on is 725mb
* **CPU:** 32-bit, i686
* **Video:** VGA text mode
* **Firmware:** BIOS/Legacy boot

### Memory Layout
| Section | Start Address | Size |
| :--- | :--- | :--- |
| Multiboot | 0x00100000 | 12 Bytes |
| Kernel Text | 0x0010000C | ~ [Size] |
| Stack | [Defined in boot.s] | 16 KB |

### Compilation Flags
* `-ffreestanding`: Build without standard library.
* `-nostdlib`: Prevent linking standard C startup files.
* `-fno-stack-protector`: Disable stack smashing protection (requires kernel support).

---

## How To Get ISO

### Download and Permissions
**In Terminal:**
* `git clone https://github.com/Millenium2133/GordOS.git`
* `sudo chmod -R 777 GordOS/`
* `cd GordOS`

### Make Sure The Cross-Compiler Is Ready
**In Terminal:**
* `export PREFIX="$HOME/opt/cross"`
* `export TARGET=i686-elf`
* `export PATH="$PREFIX/bin:$PATH"`
* `export PATH="$HOME/opt/cross/bin:$PATH"`
**Test With**
* `$TARGET-gcc --version`
* You shoukd see `i686-elf-gcc (GCC) x.x.x`
* If not, then oh well.


### Run The `make` Commands
**You may need `sudo` for some of these**
* `make` may give an error the first time you run it. Give it another go if that happens. I do not know why that happens, but I also do not care*
 * `make`: links compiles everything for you.
 * `make iso`: makes the ISO file, it is in the same directory as everything else. Feel free to move it.
 * `make clean`: deletes everything make and make iso.


## You now have an ISO, feel free to `dd` it to a USB or hard disk, or maybe even burn it to a DVD or CD.or you could be a normal person and use qemu
