#ifndef SCREEN_H
#define SCREEN_H

// call once, as soon as the multiboot2 framebuffer tag has been parsed
// (before any print_* call). addr/pitch/width/height/bpp are exactly
// what GRUB reported - we never assume they match what boot.asm's
// framebuffer request tag asked for, since GRUB picks the closest mode
// the real hardware actually supports.
//
// only bpp == 32 and bpp == 24 (RGB, framebuffer_type == 1) are drawn;
// anything else (e.g. GRUB fell back to an indexed/EGA mode) leaves the
// screen blank rather than risk misinterpreting the pixel format.
void screen_init(unsigned long long addr, unsigned int pitch,
                  unsigned int width, unsigned int height, unsigned int bpp);

#endif
