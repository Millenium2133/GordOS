GordOS is a hobbyist operating system built from scratch in C and x86 Assembly. This Operating System was made to learn the fundamentals of operating system development from a hobbyist perspective. GordOS is not intended for any form of real world use at the current moment.

---

*This documentation goes over the features, ongoing development status and basic instructions on how to actually boot the system. There is more documentation for people who would like to contribute to this project, there will be a small segment for that at the end of this document. With all that being said, GordOS should be used at your own risk. I am not responsible for any damage caused by incorrect use. If you have any issues with GordOS, please report them in the Issues tab.*

---

## Chapters
1. [What GordOS Can Currently Do](#What-GordOS-Can-Currently-Do)
2. [Current Shell Commands](#Current-Shell-Commands)
   1. [Built-In User Programs](#Built-In-User-Programs)
3. [Development Status](#Development-Status)
   1. [The Plan](#The-Plan)
   2. [Phase 1: libc](#Phase-1-libc)
   3. [Phase 2: Kernel Cleanup](#Phase-2-Kernel-Cleanup)
   4. [Phase 3: Init Program](#Phase-3-Init-Program)
   5. [Phase 4: Port Existing User Programs](#Phase-4-Port-Existing-User-Programs)
   6. [Phase 5: Build System and The Rename](#Phase-5-Build-System-and-The-Rename)
   7. [Phase 6: TCC Compiler Port](#Phase-6-TCC-Compiler-Port)
   8. [Phase 7: Window Manager](#Phase-7-Window-Manager)
4. [Technical Specifications](#Technical-Specifications)
   1. [Build Requirements](#Build-Requirements)
   2. [Hardware Requirements](#Hardware-Requirements)
   3. [Memory Layout](#Memory-Layout)
   4. [Syscall Table](#Syscall-Table)
5. [Building And Running GordOS](#Building-And-Running-GordOS)
   1. [Install The Dependencies](#Install-The-Dependencies)
   2. [Clone The Repository](#Clone-The-Repository)
   3. [Set Up The Cross-Compiler](#Set-Up-The-Cross-Compiler)
   4. [Build And Run](#Build-And-Run)
6. [Known Issues](#Known-Issues)
7. [Contributing](#Contributing)

## Other Documentation
* [Local LLM's](Local%20LLM's.md)
* [Visual Studio Code](Visual%20Studio%20Code.md)
* [Codebase requirements](Codebase%20requirements.md)

## What GordOS Can Currently Do

- Boots via GRUB on real x86 hardware and QEMU.
- Higher half kernel running at `0xC0000000`.
- VGA text mode terminal with colour, scrolling and a hardware cursor.
- PS/2 keyboard driver with full scancode translation.
- Ring 3 userspace shell `USH`. (launches automatically at boot.)
- Interactive shell with command history, cursor movement and tab autocomplete.
- Tab autocomplete on both commands, filenames and directories.
- Shell keyboard shortcuts: Ctrl+L clears the screen, Ctrl+C cancels the current line/running command.
- Physical memory manager with bitmap allocator.
- Kernel heap allocator (kmalloc/kfree) with splitting and coalescing.
- ATA PIO disk driver.
- VFS Layer `fs/vfs.h` is a driver-agnostic filesystem interface.
- FAT32 filesystem, includes mount, list, read, create, write delete and rename files and directories.
- FAT32 Long Filename (LFN) support; allows filenames up to 255 characters.
- Lowercase filenames.
- Subdirectory navigation with absolute and relative path support.
- Page fault handler.
- PIT driver at 1000Hz.
- RTC driver reading real clock time from CMOS.
- Syscall interface via `int 0x80` with return values in `eax`.
- Faulting user processes are killed and control returns to the shell.
- COM1 serial debugging console where all terminal output is mirrored.
- Ring 3 GDT segments, TSS and `jump_to_usermode`.
- Process structures with per process page directories and kernel stacks.
- Preemptive round-robin scheduler: PIT-driven context switches between a kernel task, foreground and background processes.
- Foreground `exec` and background `bg` execution with `ps` and `kill`.
- Process remapping meaning that exited and killed processes are free'd by the kernel task with no memory leaks.
- `fork()` (full eager address-space copy) and in place `exec()` that keeps the PID.
- `wait()` / `waitpid()` ; a process can spawn children and block until they exit, then collects the exit code.
- File-descriptor I/O.
- Blocking `read()`, a process waiting on the keyboard sleeps off the run queue and is woken on input.
- Standard streams (stdin/stdout/stderr as fds 0/1/2) with `write`/`read` routed through them.
- Writable file descriptors and `dup2`, enabling `cmd > file` style output redirection.
- Argument passing (argc/argv) to user programs across both `exec`/`bg` and `SYS_EXEC`.
- Keyboard focus handoff (`SYS_GIVE_FOREGROUND`), the shell hands the terminal to the inactive children and reclaims it when they exit.
- Anonymous pipes (`SYS_PIPE`), Blocks read/write and automatic EOF on last write-end close.

---

## Current Shell Commands

GordOS has quite the handful of shell commands built in, with more to come. Here is a list of all the current commands and what they do.

**Filesystem commands**
- mkdir (directory name) = makes a new directory.
- rename (file name or directory name) (new file name or directory name) = changes the name of a file or directory.
- ls (/path/to/dir) = lists everything in the specified path.
- cd (/path/to/dir) = Changes directory to the specified path.
- cat (file.txt) = prints the contents of the file.
- touch (file.txt) = creates a single empty file .
- rm (file.txt) = removes specified file.
- write (any text you want) (file.txt) = Writes text to a specified file.
- pwd = Prints the working directory (The current directory you are in.)

**Useful Commands**
- clear = Clears the screen.
- echo (literally anything) = prints the text back to the screen, more features for that is in the works.
- help = Prints out a list of commands and what they do in the terminal.
- fasterfetch = the faster version of fastfetch because its programmed directly into the OS.
- peter = Prints out a picture of Peter Griffin on the terminal.
- exec (Name-of-user-program) = executes the specified user program.
- bg (Name-of-user-program) = executes the specified user program in the background.
- ps = View all currently running processes.
- kill (name-of-user-program) = Kills the specified user program.
- time = prints current date and time.
- uptime = Prints how long the system has been on  for.
- free = Shows amount of free system memory available.
- reboot = reboots the system.

---

## Built-In User Programs

Inside of GordOS, there is a set of built-in sample programs for you to try out, these include:

- hello
- echo
- files
- counter
- forktest
- fdcat
- redir
- cat2

You can run these programs by using the `exec` command.

---

## Development Status

Development status of GordOS is currently ongoing. Updates will mainly be coming in fortnightly. Bug fixes may come in sooner.

### The Plan

The long term goal for GordOS is a clean Unix-style separation between the kernel and userland. The kernel should do the absolute minimum, CPU initialisation, memory detection, core hardware drivers, process management and syscalls, then hand off to userspace as fast as possible. Everything else lives in userspace.

When this split is complete, GordOS will be renamed. The kernel will become **Gord**, and the userland packages will live in separate repos under a **gord-packages** umbrella. Each package will be its own repo so people who only want one package don't have to clone all of them. Packages will be cloned inside the Gord kernel folder:

```
GordOS/          ← kernel repo (will become Gord/)
    gord-libc/   ← clone packages here
    gord-init/
    gord-ush/
    ...
```

The window manager will be an optional package — users who want a GUI can install it, users who don't are unaffected.

The steps to get there, in order:

---

### Phase 1: libc (`gord-libc`)

The most important piece. Everything else depends on it, init, ush, the TCC port, and the window manager client library all need a shared C library. This will live in its own repo (`gord-libc`) and produce a `libc.a` static library that all user programs link against.

**1.1 Syscall wrappers**
A single `syscall.h` with all `SYS_*` constants shared between the kernel and userland. One wrapper function per syscall (`write()`, `read()`, `fork()`, `exec()`, `exit()`, `open()`, `close()`, `read_fd()`, `write_fd()`, `dup2()`, `pipe()`, `wait()`, `waitpid()`, `getpid()`, `sleep()`, `chdir()`, `getcwd()`, `mkdir()`, `rmfile()`, `rename()`, `listdir()`, `uptime()`, `meminfo()`, `kill_pid()`, `ps()`, `setcolor()`, `findprefix()`, `readraw()`, `gettime()`, `clear()`) so the `int 0x80` + register-loading pattern exists in exactly one place. If a syscall number ever changes, only one file needs updating. `sys_readfile` and `sys_writefile` wrappers will not be included — fd-based API only.

**1.2 sbrk syscall**
A new `SYS_SBRK` syscall in the kernel that grows the calling process's heap by mapping new pages into its address space and returning the old heap break. The libc side exposes a `sbrk()` wrapper. This is what backs userland `malloc`.

**1.3 Userland malloc/free**
A port of the existing `memory/kmalloc.c` coalescing free-list allocator, backed by `sbrk()` instead of `pmm_alloc_page()`. Lives in `gord-libc/src/malloc.c`.

**1.4 String functions**
`memcpy`, `memset`, `memmove`, `strcmp`, `strcpy`, `strncpy`, `strncmp`, `strchr`, `strrchr`. Based on the existing `lib/string.c` with care taken that nothing assumes kernel privilege.

**1.5 Minimal stdio**
`printf` and `fprintf` backed by the `write()` syscall wrapper. `fopen`/`fclose`/`fread`/`fwrite` backed by fd syscalls. This is needed for the TCC port.

**1.6 crt0**
A small `crt0.s` assembly stub that receives control from the kernel, sets up `argc`/`argv`, calls `main()`, then calls `exit()` with the return value. This replaces the hand-rolled entry points currently in each user program.

**1.7 Build system**
Makefile produces `libc.a` and installs headers to `include/`. All user programs link against `libc.a` instead of rolling their own stubs. Install target uses `GORD_DISK ?= ../disk.img` so it works out of the box with the recommended folder layout.

---

### Phase 2: Kernel Cleanup

Work that happens inside the GordOS kernel repo alongside or after Phase 1.

**2.1 Single shared syscall header**
Move `SYS_*` constants out of `kernel/syscall.h` into a header shared with gord-libc. Remove duplicate definitions from all user programs.

**2.2 Remove whole-file syscalls**
Migrate any remaining callers of `sys_readfile`/`sys_writefile` in the kernel shell to fd-based equivalents, then remove `SYS_READFILE` and `SYS_WRITEFILE` from `kernel/syscall.c` and the syscall table entirely.

**2.3 Add sbrk syscall**
`sys_sbrk(size)` extends the calling process's heap by `size` bytes using `paging_map_page_in`. Returns the old heap break (standard Unix `sbrk` contract).

**2.4 Kernel shell becomes debug-only**
Gate `kernel/shell.c` behind a compile-time flag (`#ifdef GORD_DEBUG_SHELL`). Normal builds exec PID 1 unconditionally with no fallback.

---

### Phase 3: Init Program (`gord-init`)

A minimal PID 1 init program in its own repo. The kernel will exec `/bin/init.elf` unconditionally at boot.

**3.1 What init does**
- Forks and execs `ush` as the shell
- Loops forever calling `wait()` to reap any orphaned children. This is critical, any process that exits and whose parent is dead gets reparented to PID 1, so init must always be waiting or the process table fills up with zombies
- If the shell exits unexpectedly, restarts it rather than panicking

**3.2 Filesystem convention**
Establish `/bin/` as the standard location for essential binaries. `init.elf` at `/bin/init.elf`, `ush.elf` at `/bin/ush.elf`.

**3.3 Kernel side**
`kernel/kernel.c` execs `/bin/init.elf` as PID 1 unconditionally in release builds. The kernel debug shell is only available when compiled with `GORD_DEBUG_SHELL`.

---

### Phase 4: Port Existing User Programs

Move everything out of `user/` in the kernel repo into their own repos, rewritten to use gord-libc.

**4.1 `gord-ush`**
Move `user/ush.c` into its own repo. Rewrite to use libc wrappers instead of hand-rolled syscall stubs. Links against `gord-libc`.

**4.2 Sample programs**
Each sample program (`hello`, `echo`, `files`, `counter`, `forktest`, `fdcat`, `redir`, `cat2`) moves to a `gord-utils` repo. All rewritten to use libc.

**4.3 fasterfetch and peter**
These stay as kernel syscalls for now. Once framebuffer support exists in Phase 7, they will be rewritten as ordinary user programs and removed from the kernel.

---

### Phase 5: Build System and The Rename

**5.1 Update `.gitignore`**
Add `gord-libc/`, `gord-init/`, `gord-ush/`, `gord-utils/` etc. so the kernel repo does not try to track package folders.

**5.2 Update Makefile**
Remove user program build targets from the kernel Makefile. `make disk` creates and formats the image only. Package installation is handled per-package via their own `make install`.

**5.3 Create the package index**
A `gord-packages` repo containing nothing but an index listing all known packages, their repo URLs and dependencies. No code, just a registry so users can discover what exists.

**5.4 The rename**
- Rename the GitHub repo from `GordOS` to `Gord`
- The kernel binary becomes `Gord`
- The combined bootable system retains the name `GordOS`
- Update README, Makefile and grub.cfg accordingly

---

### Phase 6: TCC Compiler Port

The goal is to run a C compiler inside GordOS itself. TCC (Tiny C Compiler) is the most realistic candidate given its small size and portability. This phase cannot start until Phase 1 is solid, since TCC needs malloc, file I/O and string functions.

**What TCC needs from GordOS:**
- `malloc`/`free` — covered by Phase 1
- `fopen`/`fclose`/`fread`/`fwrite` — covered by Phase 1 stdio
- `string.h` subset — covered by Phase 1
- `sbrk` — covered by Phase 2
- A minimal include directory on the FAT32 disk (`stdint.h`, `stddef.h`, libc headers)

**Milestone:** compile a hello world C program from inside GordOS itself. Once this works, new GordOS programs can be written and compiled on the system without needing the cross-compiler on a Linux host.

---

### Phase 7: Window Manager

The window manager is an optional package. Users who want a GUI can install it, users who don't are unaffected. It will live in its own `gord-wm` repo.

**7.1 Prerequisite: Linear Framebuffer via Multiboot**
Add `MULTIBOOT_FLAG_VIDEO` to `boot.s` with `mode_type=0, width=1024, height=768, depth=32`. Extend `multiboot_info_t` in `multiboot.h` with the VBE/framebuffer fields (drivers/config, bootloader-name/APM fields must be included as padding to keep the struct layout correct even though we don't use them). Map the physical framebuffer address into kernel address space using the existing paging code. Run a solid colour fill test in QEMU with `-vga std` before touching real hardware.

**7.2 PS/2 Mouse Driver**
There is a PS/2 keyboard on IRQ1 in `keyboard.c`. The PS/2 mouse lives on IRQ12, the second PS/2 port. IRQ12 is structurally identical to `keyboard.c`'s pattern (`irq_register`, read `inb(0x60)`, maintain state across interrupts) but with 8042 controller setup first:
- `outb(0x64, 0xA8)` — enables the auxiliary device (the mouse)
- `outb(0x64, 0xD4)` — before each byte sent to the mouse, routes it to the aux port instead of the keyboard
- Send `0xF6` (set defaults) then `0xF4` (enable data reporting) to the mouse itself
- IRQ12 handler accumulates a 3-byte packet: byte 0 = button/sign flags, byte 1 = signed X delta, byte 2 = signed Y delta. Scroll wheel support (4th byte) can come later.

**7.3 Shared Memory Syscalls**
Pipes are wrong for window contents — copying a full frame through a byte-stream syscall every redraw is both slow and semantically mismatched. Two new syscalls are needed:
- `sys_shm_create(size)` — allocates physical pages via `pmm_alloc_page`, returns an opaque handle
- `sys_shm_map(handle)` — maps those physical pages into the calling process's page directory at a free virtual address, returns the address to the caller

This is a direct extension of what already exists: `fork()`'s eager address-space copy and `exec()`'s `PT_LOAD` mapping already insert page-table entries into a process's directory.

**7.4 Compositor Architecture**
One privileged compositor process owns the real framebuffer. Every client renders into its own shared memory backing buffer. The compositor puts them together. Not every process draws straight to the LFB — that has no answer for overlapping windows or clipping.

- Client requests a window: gets back a shm handle sized to its window dimensions, maps it, draws into it with plain memory writes
- Client notifies the compositor of changes via a pipe write of a small fixed struct (window id + optional damage rect), control message not pixel data, so pipe overhead is irrelevant
- Compositor, once per PIT tick, blits each window's shm buffer into the real LFB back to front
- Mouse/keyboard IRQ handlers feed the compositor

**Suggested order within Phase 7:**
1. LFB via multiboot. Solid fill, then gradient, running purely in the kernel.
2. Mouse driver and cursor sprite tracking movement, purely in the kernel.
3. Shared memory syscalls and a two-process read/write test.
4. Minimal compositor — one hardcoded window, one client writing a static pattern into shm, compositor blits it to the LFB every tick.
5. Multiple windows, z-order list, overlap clicking, click-to-focus hit testing.
6. Window decorations — title bar, drag to move.
7. Event routing — keyboard forwarded to focused client, proper per-window event queues.
8. Client-side library wrapping the IPC protocol, shipped as part of `gord-wm`.

*Each step should produce something visible in QEMU before moving to the next.*

---

## Technical Specifications

This part lists out all the technical specifications of GordOS, Please read through the build requirements for the [OSDev cross-compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler)

### Build Requirements

| Item         | Detail                                                                                   |
| :----------- | :--------------------------------------------------------------------------------------- |
| Architecture | i686 (32-bit)                                                                            |
| Compiler     | `i686-elf-gcc` ([OSDev cross-compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler)) |
| Assembler    | `i686-elf-as` (covered in the same guide)                                                |
| Optimization | `-O2`                                                                                    |
| ISO tool     | `xorriso`                                                                                |
| Disk tool    | `mtools`                                                                                 |

---

### Hardware Requirements

| Component | Requirement |
| :--- | :--- |
| CPU | 32-bit i686 |
| RAM | 725 MB minimum (tested, could be lower) |
| Video | VGA text mode |
| Firmware | BIOS / Legacy boot |
| Input | PS/2 keyboard (Scan Code Set 1) |
| Storage | ATA disk (required for filesystem) |

---

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

---

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

## Building And Running GordOS

This part is a guide to help you build and run GordOS for yourself.

*This guide assumes you are on Linux. Before you continue, it is ABSOLUTELY necessary to be comfortable with the terminal and have a basic understanding of building packages from source code. If you are not comfortable with any of those things, then this is not for you.*

Before we begin compiling GordOS, we need to build the Cross Compiler as this is an x86 system. You will not be able to do this without a cross compiler.

---

### Install The Dependencies

Before we begin building the cross compiler, there are some tools you will need for GordOS to compile properly.

| Tool | What is it for? | Installing it |
| :--- | :--- | :--- |
| xorriso | Making a bootable disk image | apt, pacman, dnf |
| mtools | Read, write and manipulate MS-DOS filesystems without needing to mount them | apt, pacman, dnf |
| grub-pc-bin | Provides GRUB bootloader modules for traditional PC's using legacy BIOS | apt, pacman (just grub), dnf (grub2-pc-bin) |
| grub-common | Shared admin files, tools and infrastructure used by GRUB | apt, pacman (just grub), dnf (grub2-common) |
| qemu-system | Kernel virtualisation for the OS to run | apt, pacman (qemu-full), dnf (qemu-system-x86) |
| qemu-utils | Provides qemu-img, used to create the raw disk.img file | apt (pacman/dnf bundle this into the main qemu package) |
| dosfstools | Provides mkfs.fat, used to format disk.img as FAT32 | apt, pacman, dnf |

Test to make sure they installed correctly before moving on.

### Clone The Repository

Make a folder on your system for GordOS, cd into that directory, then clone the repo:

```bash
git clone https://github.com/Millenium2133/GordOS.git
cd GordOS
```

Make sure you can still see all the correct files and nothing is corrupt.

### Set Up The Cross-Compiler

Once you have confirmed everything is good, it's time to move on to setting up the cross compiler.

Follow the [OSDev cross-compiler guide](https://wiki.osdev.org/GCC_Cross-Compiler), then add to your `~/.bashrc`:

```bash
export PREFIX="$HOME/opt/cross"
export TARGET=i686-elf
export PATH="$PREFIX/bin:$PATH"
```

Test to make sure the compiler is working by running the following command:

```bash
$HOME/opt/cross/bin/$TARGET-gcc --version
```

### Using Visual Studio Code

To use Visual Studio Code with GordOS, you can follow the [Visual Studio Code](Visual%20Studio%20Code.md) guide.

### Build And Run

After all tests complete, it is now time to actually compile and run GordOS. This is actually all quite simple due to the makefile.

```bash
make        # Compile and link the kernel
make iso    # Create the bootable ISO
make user   # Build the sample user programs
make disk   # Create a fresh FAT32 disk image with user programs installed
make run    # Boot in QEMU
make clean  # Remove all build artifacts
```

There is also an automated boot test script. This script boots the ISO headless in QEMU and checks the serial log for expected output covering: ring-3 execution, crash recovery, background processes, fork/exec/wait, fd reads, output redirection, the userspace shell, and long filename creation.

```bash
tools/boot-test.sh
```

---

## Known Issues

- Kernel heap is limited to the identity-mapped low 4MB of physical memory
- SFN collision resolution uses `~1` suffix only; multiple files with the same first-6-char base will alias (rare in practice)

---

## Contributing

If you would like to contribute to GordOS, please read through the [Codebase requirements](Codebase%20requirements.md), [Visual Studio Code](Visual%20Studio%20Code.md) and [Local LLM's](Local%20LLM's.md) pages before getting started. Any help is greatly appreciated.
