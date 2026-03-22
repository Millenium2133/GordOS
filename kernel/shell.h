#ifndef SHELL_H
#define SHELL_H

#define KEY_UP		0x80
#define KEY_DOWN	0x81
#define KEY_LEFT	0x82
#define KEY_RIGHT	0x83

void shell_init(void);
void shell_handle_char(char c);

#endif
