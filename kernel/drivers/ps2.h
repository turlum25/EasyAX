#ifndef PS2_H
#define PS2_H

// non-ASCII codes pushed into the key buffer for special keys.
// chosen to not collide with any printable ASCII, \n, \b, or ESC.
#define KEY_ARROW_UP   0x01
#define KEY_ARROW_DOWN 0x02

// init ps2 keyboard (call once, after idt_init())
void ps2_init();

// keyboard interrupt handler - called from the IRQ1 stub.
// Must always return normally; never block or loop here.
void keyboard_handler();

// pop the next buffered keypress (ASCII, or a KEY_* code above),
// or 0 if none waiting. safe to poll from the shell / main loop.
char key_buffer_pop(void);

#endif