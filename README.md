# MyOS
I am teaching myself OS Development as a hobby, I am by no means a professional.

The end goal of this system is to have a bootable OS that can make, edit and delete direcories and files.
Nothing too drastic, pretty basic.
I may add some other functions later.

I am reading the OSDev Wiki and YouTube Tutorials to Frankenstein together my own system.

Feel free to modify and add to this repository

# HOW TO COMPILE/INSATLL
LINUX ONLY

You will need a gcc cross compiler. https://wiki.osdev.org/GCC_Cross-Compiler.
Test the compiler with '$HOME/opt/cross/bin/$TARGET-gcc --version'
If you do everything in the OSDev wiki, you should be fine.

I would reccomend creating a new directory for the OS.
'sudo mkdir os'
'cd os'
you can name this whatever you want

clone the repo into you 'os' directory

run the make commands
'make'
'make iso'
You may need 'sudo' for some of the make commands

You can use the 'make clean' command after generating the ISO.
I would reccomend moving the ISO do a different directory before running the 'make clean' command.
You may need 'sudo' for the 'make clean' command.

# RUNNING THE SYSTEM
In qemu
'qemu-system-i386 -cdrom myos.iso'
or
'qemu-system-i386 -kernel myos'

On real hardware
'sudo dd if=myos.iso of=/dev/sdx && sync'
where 'x' is the name of your drive.

# Questions?
"Will you ever change the name from MyOS?"
Probably not. I'm lazy.

"Why should I use MyOS?"
You shouldn't.

"Why are you making MyOS?"
Becaue I feel like it.
