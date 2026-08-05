GordOS is a hobbyist operating system built from scratch in C and x86 Assembly. This Operating System was made to learn the fundamentals of operating system development from a hobbyist perspective. GordOS is not intended for any form of real world use at the current moment.

---

*This documentation goes over the features, ongoing development status and basic instructions on how to actually boot the system. There is more documentation for people who would like to contribute to this project, there will be a small segment for that at the end of this document. With all that being said, GordOS should be used at your own risk. I am not responsible for any damage caused by incorrect use. If you have any issues with GordOS, please report them in the Issues tab.*

---


## Chapters
* [What GordOS Can Currently Do](#What-GordOS-Can-Currently-Do)
* [Current Shell Commands](#Current-Shell-Commands)
* * [Built-In User Programs](#Built-In-User-Programs)
* [Development Status](#Development-Status)
* * [Part 1 Finishing The Kernel/Userland Split](#Part-1-Finishing-The-KernelUserland-Split)
* * [Part 2 Window Manager](#Part-2-Window-Manager)
* * [Suggested Order Of Operations](#Suggested-Order-Of-Operations)
* [Technical Specifications](#Technical-Specifications)
* * [Build Requirements](#Build-Requirements)
* * [Hardware Requirements](#Hardware-Requirements)
* * [Memory Layout](#Memory-Layout)
* * [Syscall Table](#Syscall-Table)
* [Building And Running GordOS](#Building-And-Running-GordOS)
* * [Install The Dependencies](#Install-The-Dependencies)
* * [Clone The Repository](#Clone-The-Repository)
* * [Set Up The Cross-Compiler](#Set-Up-The-Cross-Compiler)
* * [Build And Run](#Build-And-Run)
* [Known Issues](#Known-Issues)
* [Contributing](#Contributing) 

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

 The next steps for GordOS is to move towards a GUI/Window manager as well as splitting the Kernel and the userland. This is something I've been wanting to do for quite some time now, and we are getting closer to that goal. 

The steps needed to get to that goal include:

### Part 1: Finishing The Kernel/Userland Split

**1.1 Eliminate the remaining ring 0 only operations**
	Two syscalls require ring 0 VGA I/O, these being `sys_fasterfetch` and `sys_peter`. These exist because right now, the only way to touch the framebuffer/VGA hardware is from inside the kernel, meaning that right now, anything graphical has to be implemented as a syscall rather than using syscalls. A full split would have `fasterfetch` and `peter` as ordinary ELF binaries in /user that call generic drawing syscalls. this will be subsumed by the framebuffer/graphics work. I will talk more about in [#Part 2 Window Manager](#Part-2-Window-Manager). Once user processes can safely map and  write to a framebuffer, `fasterfetch` and `peter` should be rewritten as normal user programs.

**1.2 Whole-file Syscalls vs fd-based Syscalls**
	Right now, there is both `sys_readfile` and `sys_writefile`(read/write an entire file in one call) *and* the fd-based set (`sys_open`/`sys_read_fd`/`sys_write_fd`/`sys_close`). The whole file versions were the original API before fd-based I/O existed and are now redundant. Every whole-file operation is expressible as `open` -> `read_fd` in a loop -> `close`. Right now, `ush` or the kernel shell's `cat`/`write` commands are most likely still calling `sys_readfile` and `sys_writefile`. We would want to migrate those call sites to the fd-based API, the removing `sys_readfile` and `sys_writefile`.

**1.3 A Real libc**
	Right now, nothing sits between user programs and `int 0x80`. Each program under `/user` hand-rolls its own inline-asm syscall stubs. This is the next thing to consolidate: A small `libc.a` or even just a `syscall.h`/`syscall.c` shared by all user programs that provides:
	- One wrapper function per syscall (`write()`,`read()`,`fork()`,`exec()`,...) so the `int 0x80` + register-loading pattern exists in exactly one place.
	- A minimal `malloc`/`free` for userland. There is already a working coalescing-free-list allocator in `memory/kmalloc.c`, the userland version needs the same algorithm, but backed by a `sbrk`-equivalent syscall (grow the process's heap by extending it's page mappings) instead of `pmm_alloc_page`.
	- Basic string/mem functions. `strlen` already exists in `lib/string.c`. What should happen here is the kernel and userland will share this file, however with sharing, we will need to be careful as so nothing inside assumes kernel privilege.

This is not optional for the window manager work.

**1.4 A Single Shared Syscall-Number Header**
	The syscall numbers (0-34) are defined in exactly one header included by both the kernel and userland, this is important as we will need to add several more for shared memory and mouse/graphics.

**1.5 Init Process Cleanup**
	Right now, boot falls back to the kernel if `USH.ELF` is missing or fails to start, Right now, it's a sensible safety net, but right now I am stuck between 2 options, Option 1 being "Have the kernel unconditionally exec a fixed binary as PID 1, full stop" (Closer to how Linux works), or Option 2 "The kernel shell is a permanent dev/debug fallback". While this is not important at the current moment, it is still worth mentioning. It can be kept the same for now.

**Part 1 is not a blocking prerequisite for part 2**
*Items 1.2 - 1.5 can happen in parallel with or after the window manager work. Item 1.1 is naturally resolved by the window manager work. For this reason, while I will focus on part 1, I am more going to be treating it as ongoing keep up*.

-----

### Part 2: Window Manager

**2.1 Prerequisite: Linear Framebuffer via Multiboot**
	We will need to add `MULTIBOOT_FLAG_VIDEO` to `boot.s`'s header with something along the lines of `mode_type=0, width=1024, height=768, depth=32`, then we will need to extent `multiboot_info_t` in `multiboot.h` with the VBE/framebuffer fields (drivers/config,bootloader-name/APM fields must be included as a padding to keep the struct layout correct even though we don't use them). We will then need to map the physical framebuffer address into a kernel address space using the existing paging code, then run a solid colour fill test.
*Before touching real hardware with this, test in QEMU with `-vga std`*

**2.2 PS/2 Mouse Driver**
There is a PS/2 keyboard on IRQ1 in `keyboard.c`, the PS/2 mouse will live on IRQ12, The second PS/2 port. IRQ12 is structurally identical to `keyboard.c`'s pattern (`irq_register`, read `inb(0x60)`, maintain state across interrupts) but with 8042 controller setup first:
- `outb(0x64, 0xA8)` = This enables the Auxiliary device, in this case, its the mouse
- `outb(0x64, 0xD4)` = Before each byte gets sent to the mouse, so the controller routes it to the aux port instead of the keyboard
- send `0xF6`(Set defaults) then `0xF4`(Enable data reporting) to the mouse itself
- IRQ12 handler accumulates a 3 byte packet: byte 0 = button/sign flags, byte 1 = signed X delta, byte 2 = signed Y delta. There is a 4th byte for scroll wheel, but, as of right now, We should just get the basics working.


**2.3 Shared Memory Syscalls**
The IPC primitive the compositor model actually needs. Pipes are wrong for window contents as copying a full frame through a byte-stream syscall every redraw is both slow and semantically mismatched. We will need to add 2 things:
- `sys_shm_create(size)` = allocates physical pages via `pmm_alloc_page`, returns an opaque handle.
- `sys_shm_map(handle)` = maps those physical pages into the calling process's page directory at a free virtual address, returns the address to the caller
This is a direct extension of things we already have: `fork()`'s eager address-space copy and `exec()`'s `PT_LOAD` mapping already insert page-table entries into a process's directory.

**2,4 Compositor Architecture**
The model is simple: *One privileged compositor process owns the real framebuffer, every client renders into its own shared memory backing buffer, the compositor puts them together*. Not every process draws straight to the LFB, that has no answer for overlapping windows or clipping.
- Client requests a window: Gets back a shm handle sized to its window dimensions, maps it, draws into it with plain memory writes (A tiny userland drawing library, `putpixel`, `fill_rect`/maybe basic bitmap font blit?)
- Client notifies the compositor that something has changed via a lightweight message, a pipe write of a small fixed struct (window id + optional damage rect) is fine here, since it's a control message, not pixel data so the pipe overhead is irrelevant.
- Compositor, once per tick (hook into existing PIT-driven scheduling), moves its window list back to front, blits each window's shm buffer into the real LBF.
- mouse/keyboard IRQ handlers fed the compositor.

### Suggested Order Of Operations
1. LFB via multiboot. Solid fill, then gradient running purely in the kernel.
2. Mouse driver + cursor sprite tracking movement running purely in the kernel.
3. Shared memory syscalls + two-process read/write test.
4. Minimal compositor, one hardcoded window, one client writing a static pattern into shm, compositor blits it to the LFB every tick just to get a feel for the pipeline without real window management yet.
5. Multiple windows, z-order list, overlap clicking, click to focus hit testing.
6. Window Decorations, Title bar, drag to move (mouse down on title bar -> track deltas -> update windows screen position each tick)
7. event routing, Keyboard forwarded to focused client, proper per window event queues instead of ad hoc pipes.
8. client side library wrapping the IPC protocol so a third window manager aware program does not hand roll steps 3-7's protocol from scratch.

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

*This guide assumes you are on Linux. Before you continue, it is ABSOLUTELY nessecary to be comfortable with the terminal and have a basic understaning of building packages from source code. If you are not comfotable with any of those things, then this is not for you.*


Before we begin compiling GordOS, we need to build the Cross Compiler as this is an x86 system. You will not be able to do this without a cross compiler.


---

### Install The Dependencies

Before we begin building the cross compiler, there are some tools you will need for GordOS to compile properly.

| Tool        | What is it for?                                                                          | Installing it                                  |
| ----------- | ---------------------------------------------------------------------------------------- | ---------------------------------------------- |
| xorriso     | making a bootable disk image                                                             | apt, pacman, dnf                               |
| mtools      | abillity to read, write and manipulate MS-DOS filesystems without needing to mount them. | apt, pacman, dnf                               |
| grub-pc-bin | Provides GRUB bootloader module for traditional PC's using legacy BIOS                   | apt, pacman (just grub), dnf (grub2-pc-bin)    |
| grub-common | Shared admin files, tools and infrastructure used by GRUB                                | apt, pacman (just grub), dnf (grub2-common)    |
| qemu-system | Kernel Virtualisation for the OS to run.                                                 | apt, pacman (qemu-full), dnf (qemu-system-x86) |
Test to make sure they installed correctly before moving on.



### Clone The Repository

Make a folder on your system for GordOS, cd into that directory, then clone the repo
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



There is also an automated boot test script. This script boots the ISO headless in QEMU and check the serial log for expected output coveting: ring-3 execution, crash recovery, background processes, fork/exec/wait/ fd reads, output redirection, the userspace shell, and long filename creation.
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
