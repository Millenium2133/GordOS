# GordOS

A hobby OS built from scratch in C and x86 Assembly, made to learn the fundamentals of OS development. GordOS is not intended for real use. It exists purely as a learning project.

> **Disclaimer:** I am not a professional. I am not responsible for any damage caused to your system. **Use at your own risk.**

---

## Milestone Reached: FAT32 Long Filename (LFN) Support

GordOS now reads and writes files with names longer than the classic 8.3 format.

- **LFN read**: `ls`, `cat`, `exec`, `cd` and every other command that touches the filesystem now assembles FAT32 LFN entry chains (UTF-16LE → ASCII) and uses the full long name for display and case-insensitive matching.
- **LFN write**: `touch`, `write`, and `mkdir` generate proper LFN entry sequences before the 8.3 SFN when the name doesn't fit in 8.3. The 8.3 alias is generated automatically (`LONGFI~1.EXT` style) for compatibility with legacy tools.
- **LFN delete / rename**: both operations mark all associated LFN entries as deleted (0xE5) before removing or recreating the SFN entry.
- **Tab completion**: filename completion now matches and returns long names.
- Files created by host tools (`mcopy`, Windows, Linux) with long names are fully readable.

This is a prerequisite step towards hosting a C compiler on GordOS.

---

## Milestone Reached: A User-Space Shell with I/O Redirection

GordOS now has standard streams and a shell that runs entirely in ring 3.

- Every process has the usual **standard streams**: fd 0 = stdin (keyboard), fd 1 = stdout, fd 2 = stderr (terminal). `write()`/`read()` go through them, so a redirected stdout transparently captures any program's output without the program knowing.
- **Writable files**: `open()` with the write flag returns a write fd; bytes written to it are committed to disk when the fd is closed.
- **`dup2()`** rewires a process's fds — the basis for redirection.
- **`user/ush.c`** is a small shell *running in user mode*: it reads a command, forks a child that execs the named program, and waits for it. Output can be redirected to a file: from inside it, `HELLO.ELF > OUT.TXT` writes HELLO's output to `OUT.TXT` instead of the screen. Launch it with `exec USH.ELF` and type `exit` to return. (`user/redir.c` is a self-contained redirection test.)

The existing kernel shell remains the default interactive shell; `ush` demonstrates that a real shell can now live in user space on top of fork/exec/wait and the fd syscalls. Running programs that read keyboard input from inside `ush`, and passing arguments to programs, are the next steps.

---

## Milestone Reached: fork / exec / wait and File Descriptors

GordOS now has the core Unix process and file primitives, so user programs can manage children and read files incrementally instead of only whole-file.

- **`fork()`** duplicates the calling process — a full eager copy of its address space — returning the child's pid to the parent and `0` to the child, which then run concurrently under the scheduler.
- **`exec()`** replaces a process's program in place, keeping the same pid: the classic fork-then-exec pattern works (the child execs a new program while the parent keeps running).
- **`wait()` / `waitpid()`** block the parent until a child exits and hand back the child's pid and exit code — and the parent genuinely sleeps (off the run queue) rather than spinning.
- **File descriptors**: `open()` / `read()` / `close()` let a program open a file and read it incrementally in chunks, resuming from a cached position in the FAT cluster chain instead of slurping the whole file each time. The whole-file convenience syscalls still work.
- **Blocking `read()`**: waiting for keyboard input now sleeps the process and wakes it when a key arrives, so other processes get the CPU in the meantime.

The sample `user/forktest.c` ties it together: it forks, the child execs `HELLO.ELF`, and the parent waits for it and prints its exit code. `user/fdcat.c` writes a multi-cluster file and reads it back 16 bytes at a time through an fd. Try `exec FORKTEST.ELF` and `exec FDCAT.ELF`.

---

## Milestone Reached: Preemptive Multitasking

GordOS now runs multiple processes concurrently. A PIT-driven round-robin scheduler time-slices between a kernel task (pid 0, which also hosts the shell and reaps exited processes), foreground programs, and background programs — each in its own address space, switched via a saved kernel context per process.

- `exec PROG` runs a program in the **foreground**: it owns the keyboard and the shell waits for it to exit before showing the next prompt.
- `bg PROG` runs a program in the **background**: it is time-sliced alongside the shell, which stays fully interactive. `ps` lists processes, `kill PID` terminates one.

Try `bg COUNTER.ELF` and keep typing — `ps`, `free`, even launching more programs — while it counts in the background. A faulting process (or `kill`) is torn down and its memory reclaimed without disturbing the rest of the system.

Earlier milestone (still true): ELF executables are loaded from the FAT32 disk — `exec` reads the binary, builds a process with its own page directory and kernel stack, loads the `PT_LOAD` segments, maps a user stack, and drops to ring 3.

---

## What GordOS Can Do

- Boots via GRUB on real x86 hardware and QEMU
- Higher-half kernel running at virtual `0xC0000000`
- VGA text mode terminal with colour, scrolling, and a hardware cursor
- PS/2 keyboard driver with full scancode translation, shift, and arrow keys
- Interactive shell with command history, mid-line cursor movement, and tab autocomplete
- Tab autocomplete for both commands and filenames/directories (including subdirectories)
- Shell keyboard shortcuts: Ctrl+L clears the screen (keeping the typed line), Ctrl+C cancels the current line
- Physical memory manager with bitmap allocator
- Kernel heap allocator (kmalloc/kfree) with splitting and coalescing
- ATA PIO disk driver
- FAT32 filesystem: mount, list, read, create, write, delete, rename files and directories
- FAT32 Long Filename (LFN) support: filenames up to 255 characters, full UTF-16LE → ASCII assembly, LFN-aware tab completion
- Subdirectory navigation with absolute and relative path support
- Page fault handler that prints the faulting address and error code
- PIT driver at 1000Hz (timer_ticks, timer_sleep)
- RTC driver reading real wall-clock time from CMOS
- Syscall interface via `int 0x80` (write, exit, getpid, read, sleep, readfile, writefile, fork, exec, wait, waitpid, open, close, read_fd, write_fd, dup2) with return values in `eax`
- Faulting user processes are killed and control returns to the shell
- COM1 serial debug console — all terminal output is mirrored (`qemu -serial stdio` or a serial cable on real hardware)
- Ring 3 GDT segments, TSS, and `jump_to_usermode`
- `paging_map_page` with user bit support for mapping user-accessible pages
- Process structures with per-process page directories and kernel stacks
- Preemptive round-robin scheduler: PIT-driven context switches between a kernel task, foreground, and background processes
- Foreground (`exec`) and background (`bg`) execution, with `ps` and `kill`
- Process reaping: exited and killed processes are freed by the kernel task with no memory leak
- `fork()` (full eager address-space copy) and in-place `exec()` that keeps the pid — the fork/exec pattern works
- `wait()` / `waitpid()`: a process can spawn children and block until they exit, collecting the exit code
- File-descriptor file I/O: `open`/`read`/`close` read a file incrementally, resuming from a cached cluster position (whole-file syscalls still available)
- Blocking `read()`: a process waiting on the keyboard sleeps off the run queue and is woken on input, instead of busy-spinning
- Standard streams (stdin/stdout/stderr as fds 0/1/2) with `write`/`read` routed through them
- Writable file descriptors and `dup2`, enabling `cmd > file` style output redirection
- A user-space shell (`user/ush.c`) that forks/execs programs and redirects their output, run with `exec USH.ELF`
- ELF executable loader (PT_LOAD segments into a process address space)
- Kernel heap and VGA mappings live in the higher half, valid in every address space
- Sample user programs (`user/hello.c`, interactive `user/echo.c`, file I/O `user/files.c`, background `user/counter.c`, `user/forktest.c`, `user/fdcat.c`, `user/redir.c`, user-space shell `user/ush.c`) built by `make user`, installed by `make disk`
- `fasterfetch` — a neofetch-style system info screen (CPU brand via CPUID, live memory and uptime, colour palette)
- Shell commands: `help`, `clear`, `echo`, `about`, `ls [path]`, `pwd`, `cat`, `touch`, `write`, `rm`, `rename`, `mkdir`, `cd`, `exec`, `bg`, `ps`, `kill`, `time`, `uptime`, `free`, `fasterfetch`, `reboot`

---

## Development Status

Active development.

**Upcoming work (roughly in order):**

**Near term**
- Argument passing to programs (argc/argv across `exec`)
- Letting a user-space shell hand keyboard focus to an interactive child, then reclaim it
- Pipes (`cmd1 | cmd2`) building on the fd and redirection plumbing

**Medium term**
- VFS layer abstracting FAT32 behind a unified file interface
- Groundwork for hosting a C compiler (tcc or similar)

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
| 0 | `sys_write` | Write a buffer to stdout (fd 1) |
| 1 | `sys_exit` | Terminate process (exit code in `ebx`) |
| 2 | `sys_getpid` | Get current process ID |
| 3 | `sys_read` | Read from stdin (fd 0); blocks, sleeping, until at least 1 byte |
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
make user   # Build the sample user programs (user/*.elf)
make disk   # Create a fresh FAT32 disk.img with the user programs installed
make clean  # Remove all build artifacts
```

### Automated boot test

`tools/boot-test.sh` boots the ISO headlessly in QEMU, drives the shell
through the QEMU monitor, and checks the serial log for expected output:
a user program running in ring 3, a crashing program being killed
cleanly, a background program running concurrently, `fork`/`exec`/`wait`
(via `FORKTEST.ELF`), incremental fd reads (via `FDCAT.ELF`), output
redirection (via `REDIR.ELF`), and a user-space shell session (via
`USH.ELF`). CI runs it on every push.

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

- Shell currently runs in ring 0, will be moved to user mode when process support is complete
- Kernel heap is limited to the identity-mapped low 4MB of physical memory
- SFN collision resolution uses `~1` suffix only; multiple files with the same first-6-char base will alias (rare in practice)