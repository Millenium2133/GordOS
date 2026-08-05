Visual Studio Code will be the IDE of choice for [GordOS](README.md) at the current moment for it's extensions as well as it's IntelliSense autocomplete that may help speed up development and can reduce bugs as well as its native Linux support.

The setup for Visual Studio Code may be a little challenging as we need to use a different compiler, specifically, the [OSDev GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler).

---

*This documentation assumes you have read the README.md and have already set up a cross compiler, including having `i686-elf-gcc` and optionally `i686-elf-gdb`  available on your `PATH`. The configs below rely on that.*

---

**Steps For Setting Up The Compiler In VS Code:**

There are 3 files that matter when it comes to setting up the  [OSDev GCC Cross-Compiler](https://wiki.osdev.org/GCC_Cross-Compiler) in Visual Studio Code (Yes, the dots . at the start of each file is important):

1.  .vscode/c_cpp_properties.json - This is required for IntelliSense to work correctly and for it to not be misleading. While technically optional, it is **HIGHLY** recommended.

2. .vscode/tasks.json - This wraps the Makefile and the build scripts so Visual Studio Code can run builds and launch QEMU without the need for a second terminal. This is optional, but without it you will lose the Ctrl+Shift+B build shortcut and may get unexpected errors.

3. .vscode/launch.json - This is for debugging inside of Visual Studio Code and is optional. If you decide not to use this, you will not be able to debug inside of Visual Studio Code and will instead have to use the terminal.

While everything here is optional and you can skip these steps and still be able to contribute, It is **HIGHLY** recommended that you do all 3 as without them you may get unexpected errors and IntelliSense will be suggesting code that does not actually work in a freestanding kernel environment.

If you choose not to follow these steps, and you get errors that you cannot explain, please do not report it, instead, follow these steps and try again. If you still get errors after following the steps on this page, then feel free to make a report.

the main .vscode file path is already in GordOS's main repo, but may need some tweaking. The full "Code" for all these files that need to be added will be listed below. Make sure you are using the same code.

.vscode/c_cpp_properties.json
```json
{
  "configurations": [
    {
      "name": "GordOS",
      "compilerPath": "${env:HOME}/opt/cross/bin/i686-elf-gcc",
      "cStandard": "c17",
      "intelliSenseMode": "gcc-x86",
      "defines": ["__i386__", "GORDOS_KERNEL"],
      "includePath": ["${workspaceFolder}/kernel/include/**"],
      "compilerArgs": ["-ffreestanding"]
    }
  ],
  "version": 4
}
```
---
.vscode/tasks.json
```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "build-gordos",
      "type": "shell",
      "command": "make",
      "args": ["-j$(nproc)"],
      "group": { "kind": "build", "isDefault": true },
      "problemMatcher": ["$gcc"]
    },
    {
      "label": "run-gordos-qemu",
      "type": "shell",
      "command": "qemu-system-i386",
      "args": ["-s", "-S", "-fda", "${workspaceFolder}/build/gordos.img", "-serial", "stdio"],
      "problemMatcher": []
    }
  ]
}
```
---
.vscode/launch.json
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug GordOS (QEMU)",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/kernel.elf",
      "miDebuggerPath": "gdb",
      "miDebuggerServerAddress": "localhost:1234",
      "cwd": "${workspaceFolder}",
      "MIMode": "gdb",
      "setupCommands": [
        { "text": "set architecture i386" }
      ]
    }
  ]
}
```
---
