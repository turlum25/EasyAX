#include "headers/print.h"
#include "headers/colors.h"
#include "headers/screen.h"
#include "idt/idt.h"
#include "drivers/ps2.h"
#include "drivers/pci.h"
#include "interrupts/pic.h"
#include "mm/pmm.h"
#include "mm/multiboot2.h"
#include "mm/heap.h"
#include "mm/paging.h"
#include "shell/headers/shell.h"
#include "initramfs.h"

#include "drivers/sleep/sleep.h"
#include "drivers/cpu/detect.h"
#include "init.h"

void kernel_start(unsigned int multiboot_magic, unsigned int multiboot_info_addr)
{
    init(multiboot_magic, multiboot_info_addr);
    while (1) {
        asm volatile("hlt");
    }
}

#define VERSION "0.01"
void kernel_main(void)
{
    print_text("\naldise : v");

    print_text(VERSION);
    print_text("\n");
    shell_start();
}
