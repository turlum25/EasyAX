#ifndef MULTIBOOT2_H
#define MULTIBOOT2_H

// Multiboot2 (not multiboot1!) magic GRUB puts in eax on entry
#define MULTIBOOT2_BOOTLOADER_MAGIC 0x36d76289

// tag types we care about, out of the multiboot2 info tag list
#define MB2_TAG_END          0
#define MB2_TAG_MMAP         6
#define MB2_TAG_FRAMEBUFFER  8

// every tag in the info list starts with this - `size` includes
// the 8 header bytes, and each tag is padded up to 8-byte alignment
typedef struct {
    unsigned int type;
    unsigned int size;
} __attribute__((packed)) mb2_tag_t;

// type 6: memory map
typedef struct {
    unsigned int type;
    unsigned int size;
    unsigned int entry_size;
    unsigned int entry_version;
    // followed by (size - 16) / entry_size entries
} __attribute__((packed)) mb2_tag_mmap_t;

typedef struct {
    unsigned long long base_addr;
    unsigned long long length;
    unsigned int type; // 1 = usable RAM, anything else = reserved/unusable
    unsigned int reserved;
} __attribute__((packed)) mb2_mmap_entry_t;

// type 8: framebuffer info
typedef struct {
    unsigned int type;
    unsigned int size;
    unsigned long long framebuffer_addr;
    unsigned int framebuffer_pitch;   // bytes per scanline
    unsigned int framebuffer_width;   // pixels
    unsigned int framebuffer_height;  // pixels
    unsigned char framebuffer_bpp;    // bits per pixel
    unsigned char framebuffer_type;   // 0=indexed, 1=RGB, 2=EGA text
    unsigned short reserved;
    // followed by colour info we don't need for RGB mode
} __attribute__((packed)) mb2_tag_framebuffer_t;

#endif
