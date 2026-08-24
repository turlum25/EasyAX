#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#define MULTIBOOT_BOOTLOADER_MAGIC 0x2BADB002
#define MULTIBOOT_INFO_MEM_MAP     0x40 // bit set in mbi->flags if mmap_* is valid

typedef struct {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int syms[4];
    unsigned int mmap_length;
    unsigned int mmap_addr;
} __attribute__((packed)) multiboot_info_t;

// NOTE: entries are NOT fixed-size - advance by (entry->size + 4) bytes,
// not sizeof(this struct). The +4 is because `size` doesn't count itself.
typedef struct {
    unsigned int size;
    unsigned long long base_addr;
    unsigned long long length;
    unsigned int type; // 1 = usable RAM, anything else = reserved/unusable
} __attribute__((packed)) multiboot_mmap_entry_t;

#endif