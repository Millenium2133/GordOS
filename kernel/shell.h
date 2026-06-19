#ifndef SHELL_H
#define SHELL_H

#define KEY_UP		0x80
#define KEY_DOWN	0x81
#define KEY_LEFT	0x82
#define KEY_RIGHT	0x83
#define KEY_TAB		0x09
#define KEY_CANCEL	0x03	// Ctrl+C: cancel current line
#define KEY_CLEAR	0x0C	// Ctrl+L: clear screen, keep input

#include <stdint.h>

void shell_init(void);
void shell_handle_char(char c);
int  shell_launch_ush(void);

// Called by the process reaper when a process has exited, so the shell
// can restore its prompt (foreground) or announce completion (bg).
void shell_on_process_exit(uint32_t pid, int foreground);

#endif
