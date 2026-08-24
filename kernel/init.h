#ifndef INIT_H
#define INIT_H

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

extern char CPUType[];

static inline void* mb2_find_tag(unsigned int info_addr, unsigned int type) {
    unsigned char* ptr = (unsigned char*)info_addr + 8;
    unsigned char* end = (unsigned char*)info_addr + *(unsigned int*)info_addr;

    while (ptr < end) {
        mb2_tag_t* tag = (mb2_tag_t*)ptr;
        if (tag->type == MB2_TAG_END) {
            break;
        }
        if (tag->type == type) {
            return tag;
        }
        unsigned int advance = (tag->size + 7) & ~7u;
        ptr += advance;
    }
    return 0;
}

static inline void init(unsigned int multiboot_magic, unsigned int multiboot_info_addr) {
    asm volatile("cli");

    mb2_tag_framebuffer_t* fb_tag = 0;
    mb2_tag_mmap_t* mmap_tag = 0;
    unsigned int fb_phys_addr = 0;
    unsigned int fb_size = 0;

    if (multiboot_magic == MULTIBOOT2_BOOTLOADER_MAGIC) {
        fb_tag = (mb2_tag_framebuffer_t*)mb2_find_tag(multiboot_info_addr, MB2_TAG_FRAMEBUFFER);
        mmap_tag = (mb2_tag_mmap_t*)mb2_find_tag(multiboot_info_addr, MB2_TAG_MMAP);
    }

    if (fb_tag && (fb_tag->framebuffer_type == 1 || fb_tag->framebuffer_type == 2)) {
        screen_init(fb_tag->framebuffer_addr, fb_tag->framebuffer_pitch,
                    fb_tag->framebuffer_width, fb_tag->framebuffer_height,
                    fb_tag->framebuffer_bpp);
        fb_phys_addr = (unsigned int)fb_tag->framebuffer_addr;
        fb_size = fb_tag->framebuffer_pitch * fb_tag->framebuffer_height;
        paging_init(fb_phys_addr, fb_size);
    } else {
        screen_init(0, 0, 0, 0, 0);
        paging_init(0, 0);
    }

    clear_screen(BLACK);

    pic_remap();
    idt_init();
    ps2_init();

    asm volatile("sti");

    if (multiboot_magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        pmm_init(0);
    } else if (!mmap_tag) {
        pmm_init(0);
    } else {
        pmm_init((unsigned int)mmap_tag);
    }

    heap_init();
    pci_scan();
    initramfs_init();
}

#endif