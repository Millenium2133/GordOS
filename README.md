# GordOS

A hobby OS built from scratch in C and x86 Assembly, made to learn the fundamentals of OS development. GordOS is not intended for real use. It exists purely as a learning project.

> **Disclaimer:** I am not a professional. I am not responsible for any damage caused to your system. **Use at your own risk.**

---

## What GordOS Can Do

- Boots via GRUB on real x86 hardware and QEMU
- Higher-half kernel running at virtual `0xC0000000`
- VGA text mode terminal with colour, scrolling, and a hardware cursor
- PS/2 keyboard driver with full scancode translation, shift, and arrow keys
- **Ring-3 user-space shell** (`ush`) launched automatically at boot — the kernel-shell fallback only activates if `USH.ELF` is missing
- Interactive shell with command history, mid-line cursor movement, and tab autocomplete
- Tab autocomplete for both commands and filenames/directories (including subdirectories)
- Shell keyboard shortcuts: Ctrl+L clears the screen (keeping the typed line), Ctrl+C cancels the current line
- Physical memory manager with bitmap allocator
- Kernel heap allocator (kmalloc/kfree) with splitting and coalescing
- ATA PIO disk driver
- **VFS layer** (`fs/vfs.h`) — a driver-agnostic filesystem interface; FAT32 is registered at boot via a `vfs_ops_t` struct; all kernel and syscall code talks only to the VFS, making future filesystem drivers a drop-in
- FAT32 filesystem: mount, list, read, create, write, delete, rename files and directories
- FAT32 Long Filename (LFN) support: filenames up to 255 characters, full UTF-16LE → ASCII, LFN-aware tab completion
- **Lowercase filenames** — names with lowercase letters correctly generate LFN entries instead of being silently uppercased
- Subdirectory navigation with absolute and relative path support
- Page fault handler that prints the faulting address and error code
- PIT driver at 1000Hz (timer_ticks, timer_sleep)
- RTC driver reading real wall-clock time from CMOS
- Syscall interface via `int 0x80` with return values in `eax`
- Faulting user processes are killed and control returns to the shell
- COM1 serial debug console — all terminal output is mirrored (`qemu -serial stdio` or a serial cable on real hardware)
- Ring 3 GDT segments, TSS, and `jump_to_usermode`
- Process structures with per-process page directories and kernel stacks
- Preemptive round-robin scheduler: PIT-driven context switches between a kernel task, foreground, and background processes
- Foreground (`exec`) and background (`bg`) execution, with `ps` and `kill`
- Process reaping: exited and killed processes are freed by the kernel task with no memory leak
- `fork()` (full eager address-space copy) and in-place `exec()` that keeps the pid
- `wait()` / `waitpid()`: a process can spawn children and block until they exit, collecting the exit code
- File-descriptor I/O: `open`/`read`/`close` read a file incrementally, resuming from a cached cluster position
- Blocking `read()`: a process waiting on the keyboard sleeps off the run queue and is woken on input
- Standard streams (stdin/stdout/stderr as fds 0/1/2) with `write`/`read` routed through them
- Writable file descriptors and `dup2`, enabling `cmd > file` style output redirection
- Argument passing (argc/argv) to user programs across both `exec`/`bg` and `SYS_EXEC`
- Keyboard focus handoff (`SYS_GIVE_FOREGROUND`): the shell hands the terminal to interactive children and reclaims it when they exit
- Anonymous pipes (`SYS_PIPE`): blocking reads/writes and automatic EOF on last write-end close
- ELF executable loader (PT_LOAD segments into a process address space)
- Sample user programs: `hello`, `echo`, `files`, `counter`, `forktest`, `fdcat`, `redir`, `cat2`
- `fasterfetch` — a neofetch-style system info graphic (CPU brand via CPUID, live memory and uptime, colour palette swatches, Gordon mascot)
- `peter` — fullscreen VGA mode-13h mascot splash with font save/restore
- Shell commands: `help`, `clear`, `echo`, `about`, `ls [path]`, `pwd`, `cat`, `touch`, `write`, `rm`, `rename`, `mkdir`, `cd`, `exec`, `bg`, `ps`, `kill`, `time`, `uptime`, `free`, `fasterfetch`, `peter`, `reboot`

---

## Development Status

Active development. Working towards hosting a C compiler.

**Near term**
- Groundwork for hosting a C compiler (tcc or similar)

**Long term**
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

### Syscall Table

| Number | Name | Description |
| :--- | :--- | :--- |
| 0 | `sys_write` | Write a buffer to stdout (fd 1) |
| 1 | `sys_exit` | Terminate process (exit code in `ebx`) |
| 2 | `sys_getpid` | Get current process ID |
| 3 | `sys_read` | Read from stdin (fd 0); blocks until at least 1 byte; echoes input |
| 4 | `sys_sleep` | Sleep for N milliseconds |
| 5 | `sys_readfile` | Read a whole file from disk into a buffer |
| 6 | `sys_writefile` | Write a buffer to a file, replacing its contents |
| 7 | `sys_fork` | Duplicate the calling process (returns child pid / 0) |
| 8 | `sys_exec` | Replace the current program with the named ELF (keeps pid) |
| 9 | `sys_wait` | Block until any child exits; returns its pid, code via `ebx` |
| 10 | `sys_waitpid` | Block until the child in `ebx` exits; code via `ecx` |
| 11 | `sys_open` | Open a file (`ecx` flags: 0 = read, 1 = write); returns an fd |
| 12 | `sys_close` | Close an fd (flushing a write fd to disk) |
| 13 | `sys_read_fd` | Read up to N bytes from an fd, resuming from its position |
| 14 | `sys_write_fd` | Write N bytes to an fd (terminal, or a file's write buffer) |
| 15 | `sys_dup2` | Make `ecx` refer to the same open file as `ebx` (redirection) |
| 16 | `sys_give_foreground` | Transfer keyboard focus to the process with pid `ebx` |
| 17 | `sys_pipe` | Create an anonymous pipe; `ebx` = user `int[2]` filled with `[read_fd, write_fd]` |
| 18 | `sys_chdir` | Change the current working directory |
| 19 | `sys_getcwd` | Copy the current working directory path into a user buffer |
| 20 | `sys_mkdir` | Create a directory |
| 21 | `sys_rmfile` | Delete a file |
| 22 | `sys_rename` | Rename a file or directory |
| 23 | `sys_listdir` | List a directory to the terminal |
| 24 | `sys_uptime` | Return system uptime in seconds |
| 25 | `sys_meminfo` | Fill a `uint32_t[2]` with `[used_mb, total_mb]` |
| 26 | `sys_kill_pid` | Terminate a process by PID |
| 27 | `sys_ps` | Print the process list to the terminal |
| 28 | `sys_setcolor` | Set the VGA terminal text colour |
| 29 | `sys_fasterfetch` | Run the graphical system-info screen (requires ring-0 VGA I/O) |
| 30 | `sys_peter` | Run the fullscreen mascot splash (requires ring-0 VGA I/O) |
| 31 | `sys_findprefix` | Fill a buffer with null-separated filename matches for tab completion |
| 32 | `sys_readraw` | Read from stdin without echo (used by the shell for line editing) |
| 33 | `sys_gettime` | Fill a `uint32_t[2]` with `[(h<<16)|(m<<8)|s, (y<<16)|(mo<<8)|d]` |
| 34 | `sys_clear` | Clear the terminal screen |

Syscall convention: number in `eax`, args in `ebx`/`ecx`/`edx`, return value in `eax`.

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

### 4. Build and Run

```bash
make        # Compile and link the kernel
make iso    # Create the bootable ISO
make user   # Build the sample user programs
make disk   # Create a fresh FAT32 disk image with user programs installed
make run    # Boot in QEMU
make clean  # Remove all build artifacts
```

### Automated boot test

`tools/boot-test.sh` boots the ISO headlessly in QEMU and checks the serial log for expected output covering: ring-3 execution, crash recovery, background processes, fork/exec/wait, fd reads, output redirection, the user-space shell, and long filename creation. CI runs it on every push.

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
mcopy -i disk.img myfile.txt ::myfile.txt
```

### Real x86 Hardware

- Build the ISO and `dd` it to a USB drive
- Format a hard drive or partition as FAT32 for filesystem support
- Boot from USB — should work out of the box on any BIOS/legacy boot system

**Notes:**
- Less stable than QEMU — things may work in emulation before they're fixed for real hardware
- Don't remove the USB while running
- Formatting a partition as FAT32 will erase its contents

> **I AM NOT RESPONSIBLE FOR ANY DATA LOSS. YOU HAVE BEEN WARNED.**

---

## Known Issues

- Kernel heap is limited to the identity-mapped low 4MB of physical memory
- SFN collision resolution uses `~1` suffix only; multiple files with the same first-6-char base will alias (rare in practice)

## Star History

<a href="https://www.star-history.com/?repos=Millenium2133%2FGordOS&type=timeline&legend=top-left">
 <picture>
   <source media="(prefers-color-scheme: dark)" srcset="https://api.star-history.com/chart?repos=Millenium2133/GordOS&type=timeline&theme=dark&legend=top-left" />
   <source media="(prefers-color-scheme: light)" srcset="https://api.star-history.com/chart?repos=Millenium2133/GordOS&type=timeline&legend=top-left" />
   <img alt="Star History Chart" src="https://api.star-history.com/chart?repos=Millenium2133/GordOS&type=timeline&legend=top-left" />
 </picture>
</a>
