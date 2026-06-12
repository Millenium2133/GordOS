#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>

void keyboard_init(void);

// Pop one buffered character (input typed while a user process is
// running). Returns -1 if the buffer is empty.
int keyboard_read_char(void);

// Discard any buffered input
void keyboard_flush(void);

#endif
