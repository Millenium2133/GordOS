#ifndef SERIAL_H
#define SERIAL_H

// COM1 serial port driver. Used as a debug console: all terminal
// output is mirrored here, visible with `qemu -serial stdio` or a
// serial cable on real hardware.

// Initialise COM1 at 38400 baud 8N1. Returns 0 on success, -1 if the
// loopback self-test fails (no working UART present).
int serial_init(void);

// Write one character (no-op if serial_init failed or wasn't called)
void serial_putchar(char c);

// Write a NUL-terminated string
void serial_writestring(const char* s);

#endif
