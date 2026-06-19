#ifndef VGA13_H
#define VGA13_H

#include <stdint.h>

// Show a fullscreen graphics splash of the GordOS mascot.
//
// Saves the current text font + DAC palette, switches the VGA to mode
// 13h (320x200x256, linear framebuffer), draws the mascot bitmap
// centered, then polls the keyboard until a key is pressed and restores
// 80x25 text mode (font, palette, and a cleared screen).
//
// Must be called with interrupts disabled (it polls the keyboard port
// directly and would race the keyboard IRQ otherwise). Returns with the
// text terminal re-initialised; the caller should redraw its prompt.
void vga_splash_gordon(void);

// Like vga_splash_gordon, but draws the mascot on the left and renders
// sysinfo text on the right — a full VGA fasterfetch.  Same interrupt
// and return-value contract as vga_splash_gordon.
void vga_fasterfetch(const char* cpu, uint32_t used_mb, uint32_t total_mb,
                     uint32_t uptime_secs);

#endif
