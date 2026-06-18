#include "keyboard.h"
#include "idt.h"
#include "pic.h"
#include "shell.h"
#include "process.h"
#include "scheduler.h"

// Ring buffer for input while a user process is running. The shell is
// suspended inside exec then, so keystrokes are stored here for
// sys_read instead of being fed to the shell.
#define KBD_BUFFER_SIZE 256
static volatile char kbd_buffer[KBD_BUFFER_SIZE];
static volatile int kbd_head = 0;
static volatile int kbd_tail = 0;

int keyboard_read_char(void)
{
    if (kbd_tail == kbd_head)
        return -1;

    char c = kbd_buffer[kbd_tail];
    kbd_tail = (kbd_tail + 1) % KBD_BUFFER_SIZE;
    return (unsigned char)c;
}

void keyboard_flush(void)
{
    kbd_tail = kbd_head;
}

// Route a decoded character. A foreground process owns the keyboard,
// so its keystrokes go into the ring buffer for sys_read; otherwise
// (idle, or only background processes running) they go to the shell.
static void deliver_char(char c)
{
    if (foreground_process)
    {
        // Shell editing keys mean nothing to a user program
        if ((unsigned char)c >= 0x80)
            return;

        int next = (kbd_head + 1) % KBD_BUFFER_SIZE;
        if (next != kbd_tail)
        {
            kbd_buffer[kbd_head] = c;
            kbd_head = next;
        }

        // If that process is asleep in sys_read, wake it (we're in the
        // IRQ with interrupts already disabled, as scheduler_wake wants)
        if (foreground_process->state == PROCESS_BLOCKED &&
            foreground_process->block_reason == BLOCK_READ)
            scheduler_wake(foreground_process);
        return;
    }

    shell_handle_char(c);
}

static const char scancode_table[128] =
{
    0,   0,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=',  0,
    0,  'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']',  0,
    0,  'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,  '\\','z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/',  0,
    '*',  0,  ' '
};

static const char scancode_table_shift[128] =
{
    0,   0,  '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0,
    0,  'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0,
    0,  'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,  '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0,  ' ', 0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,  '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.'
};

static int shift_pressed = 0;
static int ctrl_pressed = 0;
static int extended = 0;

static void keyboard_handler(struct registers* regs)
{
    (void)regs;

    uint8_t scancode = inb(0x60);

    if (scancode == 0xE0)
    {
        extended = 1;
        return;
    }

    if (scancode & 0x80)
    {
        uint8_t released = scancode & 0x7F;
        if (!extended && (released == 0x2A || released == 0x36))
            shift_pressed = 0;
        if (released == 0x1D)  // left or (extended) right control
            ctrl_pressed = 0;
        extended = 0;
        return;
    }

    if (extended)
    {
        extended = 0;
        switch (scancode)
        {
            case 0x1D: ctrl_pressed = 1;        return;  // right control
            case 0x48: deliver_char(KEY_UP);    return;
            case 0x50: deliver_char(KEY_DOWN);  return;
            case 0x4B: deliver_char(KEY_LEFT);  return;
            case 0x4D: deliver_char(KEY_RIGHT); return;
        }
        return;
    }

    if (scancode == 0x1D) { ctrl_pressed = 1; return; }  // left control
    if (scancode == 0x2A || scancode == 0x36) { shift_pressed = 1; return; }
    if (scancode == 0x0F) { deliver_char(KEY_TAB); return; }
    if (scancode == 0x1C) { deliver_char('\n'); return; }
    if (scancode == 0x0E) { deliver_char('\b'); return; }

    // Ctrl held: turn a letter into its control code (Ctrl+L -> 0x0C,
    // Ctrl+C -> 0x03, etc.) and skip the normal character path
    if (ctrl_pressed)
    {
        char base = scancode_table[scancode];
        if (base >= 'a' && base <= 'z')
            deliver_char(base & 0x1F);
        return;
    }

    char c = shift_pressed ? scancode_table_shift[scancode] : scancode_table[scancode];
    if (c != 0)
        deliver_char(c);
}

void keyboard_init(void)
{
    irq_register(1, keyboard_handler);
}
