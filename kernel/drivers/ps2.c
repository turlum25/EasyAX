#include "ps2.h"
#include "../headers/io.h"
#include "../headers/print.h"

// PS/2 controller ports
#define PS2_DATA_PORT   0x60
#define PS2_STATUS_PORT 0x64

// Scan Code Set 1, "make" codes (bit 7 clear). Index = scancode,
// value = ASCII char with no modifiers held (0 = unmapped).
static const char scancode_to_ascii[128] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', // 0x00-0x09
    '9', '0', '-', '=', '\b','\t','q', 'w', 'e', 'r', // 0x0A-0x13
    't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,   // 0x14-0x1D (0x1D = left ctrl)
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', // 0x1E-0x27
    '\'','`', 0,  '\\','z', 'x', 'c', 'v', 'b', 'n',   // 0x28-0x31 (0x2A = left shift)
    'm', ',', '.', '/', 0,   '*', 0,   ' ', 0,   0,    // 0x32-0x3B (0x36 = right shift)
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    // 0x3C-0x45 (function keys, numlock, etc)
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    // 0x46-0x4F
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    // 0x50-0x59
    0,   0,   0,                                       // 0x5A-0x5C
};

// same layout, but the character produced when Shift is held.
// for letters this is just the uppercase form; for symbol keys it's
// whatever the shifted glyph is (e.g. '1' -> '!').
static const char scancode_to_ascii_shifted[128] = {
    0,   27,  '!', '@', '#', '$', '%', '^', '&', '*', // 0x00-0x09
    '(', ')', '_', '+', '\b','\t','Q', 'W', 'E', 'R', // 0x0A-0x13
    'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n', 0,   // 0x14-0x1D
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', // 0x1E-0x27
    '"', '~', 0,  '|', 'Z', 'X', 'C', 'V', 'B', 'N',   // 0x28-0x31
    'M', '<', '>', '?', 0,   '*', 0,   ' ', 0,   0,    // 0x32-0x3B
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    // 0x3C-0x45
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    // 0x46-0x4F
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,    // 0x50-0x59
    0,   0,   0,                                       // 0x5A-0x5C
};

// simple ring buffer so key presses can be consumed elsewhere (e.g. shell)
#define KEY_BUFFER_SIZE 256
static volatile char key_buffer[KEY_BUFFER_SIZE];
static volatile unsigned int key_buf_head = 0; // next write index
static volatile unsigned int key_buf_tail = 0; // next read index

// extended (0xE0-prefixed) scancodes we care about right now
#define SCANCODE_EXTENDED_PREFIX 0xE0
#define SCANCODE_ARROW_UP        0x48
#define SCANCODE_ARROW_DOWN      0x50

// modifier key scancodes (Scan Code Set 1, base codes without the
// release bit set)
#define SCANCODE_LSHIFT   0x2A
#define SCANCODE_RSHIFT   0x36
#define SCANCODE_CAPSLOCK 0x3A

// set when the previous byte from the controller was the 0xE0
// "extended key follows" prefix
static int extended_prefix = 0;

// modifier state
static int shift_held   = 0;
static int capslock_on  = 0;

static void key_buffer_push(char c)
{
    unsigned int next = (key_buf_head + 1) % KEY_BUFFER_SIZE;

    // drop the char if the buffer is full instead of overwriting
    // unread data
    if (next == key_buf_tail) {
        return;
    }

    key_buffer[key_buf_head] = c;
    key_buf_head = next;
}

// returns 0 if no key is waiting
char key_buffer_pop(void)
{
    char c;

    if (key_buf_tail == key_buf_head) {
        return 0;
    }

    c = key_buffer[key_buf_tail];
    key_buf_tail = (key_buf_tail + 1) % KEY_BUFFER_SIZE;
    return c;
}

void ps2_init()
{
    // drain anything left sitting in the PS/2 output buffer from
    // BIOS/GRUB so we start from a clean state
    while (inb(PS2_STATUS_PORT) & 0x01) {
        inb(PS2_DATA_PORT);

        outb(0x80, 0);
    }
}

// called from keyboard_stub (idt_asm.asm) on every IRQ1.
// keyboard_stub sends the PIC EOI and does iretd AFTER this returns,
// so this function must always return normally - never block/loop here.
void keyboard_handler()
{
    unsigned char scancode = inb(PS2_DATA_PORT);

    if (scancode == SCANCODE_EXTENDED_PREFIX) {
        extended_prefix = 1;
        return;
    }

    int is_extended = extended_prefix;
    extended_prefix = 0;

    int is_release = (scancode & 0x80) != 0;
    unsigned char code = scancode & 0x7F; // strip the release bit

    if (is_extended) {
        if (!is_release) {
            if (code == SCANCODE_ARROW_UP) {
                key_buffer_push(KEY_ARROW_UP);
            } else if (code == SCANCODE_ARROW_DOWN) {
                key_buffer_push(KEY_ARROW_DOWN);
            }
        }
        return;
    }

    // shift is held for as long as the key is down
    if (code == SCANCODE_LSHIFT || code == SCANCODE_RSHIFT) {
        shift_held = !is_release;
        return;
    }

    // caps lock toggles once per press, ignore the release
    if (code == SCANCODE_CAPSLOCK) {
        if (!is_release) {
            capslock_on = !capslock_on;
        }
        return;
    }

    // every other key release is ignored for now
    if (is_release) {
        return;
    }

    char base    = scancode_to_ascii[code];
    char shifted = scancode_to_ascii_shifted[code];
    char c;

    if (base >= 'a' && base <= 'z') {
        // caps lock and shift both flip letter case, and cancel
        // each other out when both are active
        int use_upper = shift_held ^ capslock_on;
        c = use_upper ? shifted : base;
    } else {
        // caps lock has no effect on digits/symbols - only shift does
        c = shift_held ? shifted : base;
    }

    if (c != 0) {
        key_buffer_push(c);
    }
}