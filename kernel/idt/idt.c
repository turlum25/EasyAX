#include "idt.h"
#include "../headers/print.h"

struct idt_entry idt[256];
struct idt_ptr idtp;

extern void idt_load(unsigned int);
extern void keyboard_stub();
extern void linux_syscall_handler();
extern void* isr_stub_table[32];

void idt_set_gate(
    int n,
    unsigned int handler,
    unsigned short selector,
    unsigned char flags
)
{
    idt[n].base_low = handler & 0xFFFF;
    idt[n].selector = selector;
    idt[n].zero = 0;
    idt[n].flags = flags;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
}

void isr_exception_handler(unsigned int vector, unsigned int error_code)
{
    print_text("\nUnhandled CPU exception: ");
    print_uint(vector);
    print_text(" (error code: ");
    print_uint(error_code);
    print_text(")\nSystem halted.\n");

    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}

void idt_init()
{
    for (int i = 0; i < 256; i++) {
        idt[i].base_low = 0;
        idt[i].base_high = 0;
        idt[i].selector = 0;
        idt[i].flags = 0;
        idt[i].zero = 0;
    }

    for (int i = 0; i < 32; i++) {
        idt_set_gate(i, (unsigned int)isr_stub_table[i], 0x08, 0x8E);
    }

    idt_set_gate(33, (unsigned int)keyboard_stub, 0x08, 0x8E);
    idt_set_gate(128, (unsigned int)linux_syscall_handler, 0x08, 0xEE);

    idtp.limit = sizeof(idt) - 1;
    idtp.base = (unsigned int)&idt;

    idt_load((unsigned int)&idtp);
}