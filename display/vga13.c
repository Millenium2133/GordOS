// VGA mode 13h graphics splash for the GordOS mascot.
//
// GordOS runs in protected mode with no BIOS, so the mode switch is done
// by programming the VGA registers directly. The register tables and the
// font save/restore routines follow Chris Giese's well-known public-
// domain VGA mode-set code (https://files.osdev.org/mirrors/geezer/).
//
// Mode 13h is a 320x200x256 LINEAR framebuffer at 0xA0000 — trivial to
// plot into (one byte per pixel). The catch: that framebuffer reuses the
// same physical memory plane that holds the text font, so drawing wipes
// the font. We therefore save the font (and the DAC palette) before the
// switch and restore them after, so the text shell comes back intact.

#include "vga13.h"
#include "vga.h"
#include "pic.h"      // inb / outb
#include "string.h"   // memcpy / memset
#include "gordon_art.h"

// VGA register ports
#define VGA_AC_INDEX        0x3C0
#define VGA_AC_WRITE        0x3C0
#define VGA_MISC_WRITE      0x3C2
#define VGA_SEQ_INDEX       0x3C4
#define VGA_SEQ_DATA        0x3C5
#define VGA_DAC_READ_INDEX  0x3C7
#define VGA_DAC_WRITE_INDEX 0x3C8
#define VGA_DAC_DATA        0x3C9
#define VGA_GC_INDEX        0x3CE
#define VGA_GC_DATA         0x3CF
#define VGA_CRTC_INDEX      0x3D4
#define VGA_CRTC_DATA       0x3D5
#define VGA_INSTAT_READ     0x3DA

#define VGA_NUM_SEQ_REGS    5
#define VGA_NUM_CRTC_REGS   25
#define VGA_NUM_GC_REGS     9
#define VGA_NUM_AC_REGS     21
#define VGA_NUM_REGS (1 + VGA_NUM_SEQ_REGS + VGA_NUM_CRTC_REGS + \
                      VGA_NUM_GC_REGS + VGA_NUM_AC_REGS)

// Linear framebuffer / font memory, via the higher-half identity map of
// physical 0xA0000 (present in every address space, like the text buffer).
static volatile uint8_t* const VGA_MEM = (volatile uint8_t*)0xC00A0000;

#define TEXT_FONT_HEIGHT 16

// --- Register tables ------------------------------------------------------

static unsigned char g_mode13h[VGA_NUM_REGS] =
{
    /* MISC */ 0x63,
    /* SEQ  */ 0x03, 0x01, 0x0F, 0x00, 0x0E,
    /* CRTC */ 0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
               0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
               0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3,
               0xFF,
    /* GC   */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F,
               0xFF,
    /* AC   */ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
               0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
               0x41, 0x00, 0x0F, 0x00, 0x00
};

static unsigned char g_text80x25[VGA_NUM_REGS] =
{
    /* MISC */ 0x67,
    /* SEQ  */ 0x03, 0x00, 0x03, 0x00, 0x02,
    /* CRTC */ 0x5F, 0x4F, 0x50, 0x82, 0x55, 0x81, 0xBF, 0x1F,
               0x00, 0x4F, 0x0D, 0x0E, 0x00, 0x00, 0x00, 0x50,
               0x9C, 0x0E, 0x8F, 0x28, 0x1F, 0x96, 0xB9, 0xA3,
               0xFF,
    /* GC   */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x0E, 0x00,
               0xFF,
    /* AC   */ 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x14, 0x07,
               0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
               0x0C, 0x00, 0x0F, 0x08, 0x00
};

// Saved text-mode state, restored on the way out.
static unsigned char saved_font[256 * TEXT_FONT_HEIGHT];
static unsigned char saved_palette[256 * 3];

// --- Low-level helpers ----------------------------------------------------

static void write_regs(unsigned char* regs)
{
    unsigned char i;

    outb(VGA_MISC_WRITE, *regs++);

    for (i = 0; i < VGA_NUM_SEQ_REGS; i++)
    {
        outb(VGA_SEQ_INDEX, i);
        outb(VGA_SEQ_DATA, *regs++);
    }

    // Unlock CRTC registers 0-7 (clear protect bit in CRTC[0x11])
    outb(VGA_CRTC_INDEX, 0x03);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);
    regs[0x03] |= 0x80;
    regs[0x11] &= ~0x80;

    for (i = 0; i < VGA_NUM_CRTC_REGS; i++)
    {
        outb(VGA_CRTC_INDEX, i);
        outb(VGA_CRTC_DATA, *regs++);
    }

    for (i = 0; i < VGA_NUM_GC_REGS; i++)
    {
        outb(VGA_GC_INDEX, i);
        outb(VGA_GC_DATA, *regs++);
    }

    for (i = 0; i < VGA_NUM_AC_REGS; i++)
    {
        (void)inb(VGA_INSTAT_READ);
        outb(VGA_AC_INDEX, i);
        outb(VGA_AC_WRITE, *regs++);
    }

    // Lock 16-colour palette and unblank the display
    (void)inb(VGA_INSTAT_READ);
    outb(VGA_AC_INDEX, 0x20);
}

static void set_plane(unsigned p)
{
    unsigned char pmask;
    p &= 3;
    pmask = 1 << p;

    outb(VGA_GC_INDEX, 4);
    outb(VGA_GC_DATA, p);
    outb(VGA_SEQ_INDEX, 2);
    outb(VGA_SEQ_DATA, pmask);
}

static void read_font(unsigned char* buf, unsigned font_height)
{
    unsigned char seq2, seq4, gc4, gc5, gc6;
    unsigned i;

    outb(VGA_SEQ_INDEX, 2); seq2 = inb(VGA_SEQ_DATA);
    outb(VGA_SEQ_INDEX, 4); seq4 = inb(VGA_SEQ_DATA);
    outb(VGA_SEQ_INDEX, 4); outb(VGA_SEQ_DATA, seq4 | 0x04);
    outb(VGA_GC_INDEX, 4); gc4 = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 5); gc5 = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 5); outb(VGA_GC_DATA, gc5 & ~0x10);
    outb(VGA_GC_INDEX, 6); gc6 = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 6); outb(VGA_GC_DATA, gc6 & ~0x02);

    set_plane(2);
    for (i = 0; i < 256; i++)
        memcpy(buf + i * font_height, (const void*)(VGA_MEM + i * 32), font_height);

    outb(VGA_SEQ_INDEX, 2); outb(VGA_SEQ_DATA, seq2);
    outb(VGA_SEQ_INDEX, 4); outb(VGA_SEQ_DATA, seq4);
    outb(VGA_GC_INDEX, 4); outb(VGA_GC_DATA, gc4);
    outb(VGA_GC_INDEX, 5); outb(VGA_GC_DATA, gc5);
    outb(VGA_GC_INDEX, 6); outb(VGA_GC_DATA, gc6);
}

static void write_font(const unsigned char* buf, unsigned font_height)
{
    unsigned char seq2, seq4, gc4, gc5, gc6;
    unsigned i;

    outb(VGA_SEQ_INDEX, 2); seq2 = inb(VGA_SEQ_DATA);
    outb(VGA_SEQ_INDEX, 4); seq4 = inb(VGA_SEQ_DATA);
    outb(VGA_SEQ_INDEX, 4); outb(VGA_SEQ_DATA, seq4 | 0x04);
    outb(VGA_GC_INDEX, 4); gc4 = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 5); gc5 = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 5); outb(VGA_GC_DATA, gc5 & ~0x10);
    outb(VGA_GC_INDEX, 6); gc6 = inb(VGA_GC_DATA);
    outb(VGA_GC_INDEX, 6); outb(VGA_GC_DATA, gc6 & ~0x02);

    set_plane(2);
    for (i = 0; i < 256; i++)
        memcpy((void*)(VGA_MEM + i * 32), buf + i * font_height, font_height);

    outb(VGA_SEQ_INDEX, 2); outb(VGA_SEQ_DATA, seq2);
    outb(VGA_SEQ_INDEX, 4); outb(VGA_SEQ_DATA, seq4);
    outb(VGA_GC_INDEX, 4); outb(VGA_GC_DATA, gc4);
    outb(VGA_GC_INDEX, 5); outb(VGA_GC_DATA, gc5);
    outb(VGA_GC_INDEX, 6); outb(VGA_GC_DATA, gc6);
}

static void save_palette(unsigned char* buf)
{
    outb(VGA_DAC_READ_INDEX, 0);
    for (int i = 0; i < 256 * 3; i++)
        buf[i] = inb(VGA_DAC_DATA);
}

static void restore_palette(const unsigned char* buf)
{
    outb(VGA_DAC_WRITE_INDEX, 0);
    for (int i = 0; i < 256 * 3; i++)
        outb(VGA_DAC_DATA, buf[i]);
}

// --- The splash itself ----------------------------------------------------

static void draw_bitmap(void)
{
    int x0 = (320 - GORDON_W) / 2;
    int y0 = (200 - GORDON_H) / 2;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;

    for (int y = 0; y < GORDON_H; y++)
    {
        const unsigned char* rowp = gordon_bitmap + y * GORDON_STRIDE;
        volatile uint8_t* dst = VGA_MEM + (y0 + y) * 320 + x0;
        for (int x = 0; x < GORDON_W; x++)
        {
            if (rowp[x >> 3] & (0x80 >> (x & 7)))
                dst[x] = 1;   // mascot colour (palette index 1)
        }
    }
}

static void wait_for_key(void)
{
    // Drain anything pending, then block until a make (press) code.
    while (inb(0x64) & 1)
        (void)inb(0x60);

    for (;;)
    {
        if (inb(0x64) & 1)
        {
            uint8_t sc = inb(0x60);
            if (!(sc & 0x80))   // ignore key-release codes
                break;
        }
    }
}

void vga_splash_gordon(void)
{
    // Preserve text-mode state (font lives in the plane mode 13h reuses).
    save_palette(saved_palette);
    read_font(saved_font, TEXT_FONT_HEIGHT);

    write_regs(g_mode13h);

    // Palette: index 0 = black, index 1 = warm ginger (6-bit DAC values).
    outb(VGA_DAC_WRITE_INDEX, 0);
    outb(VGA_DAC_DATA, 0);  outb(VGA_DAC_DATA, 0);  outb(VGA_DAC_DATA, 0);
    outb(VGA_DAC_DATA, 63); outb(VGA_DAC_DATA, 40); outb(VGA_DAC_DATA, 10);

    // Clear to black and draw.
    memset((void*)VGA_MEM, 0, 320 * 200);
    draw_bitmap();

    wait_for_key();

    // Back to text: registers, font, palette, then a clean terminal.
    write_regs(g_text80x25);
    write_font(saved_font, TEXT_FONT_HEIGHT);
    restore_palette(saved_palette);

    terminal_initialize();
}
