# Philosophy
GordOS is an educational Operating System. The goal of this project is to learn operating system development while producing a clean and understandable codebase.

Contributes should prioritise:
- Correctness
- Readability
- Simplicity
- Maintainability
over clever implementations or unnecessary optimisation.

If a piece of code is difficult to explain, it's probably too complicated.

---

# General Coding Guidelines

## Follow Existing Patterns
Before adding a new subsystem or feature, look through the existing codebase for similar implementations.

Examples include:
- IRQ handlers
- Drivers
- Memory allocators
- Filesystems
- User Programs
- Syscalls

New code should feel like it belongs in GordOS rather than introducing a completely different style.


## Keep Functions Focused
Functions should perform one logical task.
Avoid functions that attempt to initialise, configure and run multiple unrelated systems.

Instead:
```C
paging_init();
kmalloc_init();
scheduler_init();
```
rather than one giant initialisation function containing thousands of lines.


---

## Prefer Straightforward C
Avoid unnecessary
- Macros
- Complicated pointer tasks
- Obscure arithmetic
- Excessive preprocessor logic
This project is intended to teach operating system development.
Readable code is more valuable then clever code.


---

## Do Not Duplicate Existing Functionality
If an existing subsystem already solves your problem. extend it instead of creating another implementation.

For example:
- Extend the VFS instead of bypassing it.
- Extent the scheduler instead of creating another scheduling mechanism.
- Extend existing Syscalls rather than Introducing redundant API's.


----

## Comments
Comments should explain why the code exists.

Good:
```C
/* Heap pointers must stay valid after switching page directories. */
```

Bad
```C
/* Increment i */
i++;
```

Comments are especially encouraged around
- Assembly
- Interrupt handlers
- Paging
- Memory management
- Scheduler code
- Hardware I/O
- Any algorithm that is not immediately obvious

----

## Naming
Keep names descriptive.

Good
```C
process_create()
keyboard_flush()
vfs_register()
```

Avoid abbreviations unless they are already common throughout the project.

Examples that are acceptable:
- IRQ
- PIT
- RTC
- ATA
- VFS
- ELF

----

## Headers
Header files should expose the public interface of a subsystem.
Implementation details should remain inside the corresponding `.c` file.
If a helper function is only used withing one source file, mark it `static`.

----

## Memory Management
All dynamically allocated kernel memory must have clear ownership.

When allocating memory:
- Know who owns it.
- Know who frees it.
- Avoid hidden ownership.
Memory leaks are considered bugs.
Never allocate memory "Just in case."

----

## Drivers
Drivers should remain responsible only for interacting with hardware.
High level logic belongs elsewhere.

For example:
- `keyboard.c` should decode keyboard input.
- The shell the interpret commands.
- The scheduler should decide which process receives input.
Keep responsibilities separate.

----

## Kernel/User Separation
Do not place functionally the kernel unless kernel privilege is genuinely required.

Whenever possible:
- Hardware access belongs to the kernel.
- Policy belongs in userspace

As GordOS continues moving towards a complete kernel/userland split, new code should support this direction rather than working against it.

----

## Error Handling
Handle errors where practical.

Check:
- Allocation failures.
- invalid pointers.
- invalid user input.
- filesystem failures.
- Invalid hardware states.
Avoid silently ignoring failures.

----

## Testing
Every contribution should:
- Compile successfully.
- Boot successfully.
- Pass the automated boot test (where applicable)
- Be manually tested if it affects runtime behaviour.
Untested code should not be merged.

----

## AI Generated Code
AI generated code is allowed.

However:
- You must understand every line.
- You must test every change.
- Generated code in expected to meet the same standards as handwritten code.
Code that cannot be explained by the contributor may be rejected.

----

## Pull Requests
Pull requests should contain one logical change.

Good examples:
- FAT32 bug fix.
- New syscall.
- Keyboard driver improvement.
- Documentation update.

Bad examples:
- Scheduler rewrite.
- Filesystem Changes.
- Documentation.
- Shell improvements.
All in one pull request.

----

## Reasons Why A Pull Request May Br Rejected
A pull request may be rejected if it:
- Duplicates existing functionality.
- Introduces unnecessary complexity.
- Does not match the architecture of GordOS.
- Is insufficiently tested.
- Contains code the contribute can't explain.
- Breaks existing behaviour.
- Ignores these requirements.