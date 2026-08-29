/*  main.c
    This is the main file which manages stuff i guess
    Starts kernel
    Starts shell
    Et cetera
    
    Dear script kiddies, this is NOT fancy Python
    Don't try importing flask here or try to be some hacker
    from Watch Dogs
    
    
    Dear loren-wastaken: I made this so people can mod main.c easily.
    
    Proudly made in Fresh editor
*/

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

#include "version.h"

void kernel_start(unsigned int multiboot_magic, unsigned int multiboot_info_addr)
{
    init(multiboot_magic, multiboot_info_addr);
    while (1) {
        asm volatile("hlt");
    }
}


void kernel_main(void) {
    // Version of kernel. Not OS.
    print_text("\naldise : v");
    print_text(KERNELVER);
    print_text("\n");
    
    // Prints CPU (drivers found in drivers/cpu/detect.h).
    DetectCPU();
    print_text("CPU: ");
    print_text(CPUType);
    print_text("\n");
    // Starts shell. Do not touch
    shell_start();
}
