#include "elf32.h"

#define EI_NIDENT 16

typedef struct {
    unsigned char  e_ident[EI_NIDENT];
    unsigned short e_type;
    unsigned short e_machine;
    unsigned int   e_version;
    unsigned int   e_entry;
    unsigned int   e_phoff;
    unsigned int   e_shoff;
    unsigned int   e_flags;
    unsigned short e_ehsize;
    unsigned short e_phentsize;
    unsigned short e_phnum;
    unsigned short e_shentsize;
    unsigned short e_shnum;
    unsigned short e_shstrndx;
} __attribute__((packed)) elf32_ehdr_t;

typedef struct {
    unsigned int p_type;
    unsigned int p_offset;
    unsigned int p_vaddr;
    unsigned int p_paddr;
    unsigned int p_filesz;
    unsigned int p_memsz;
    unsigned int p_flags;
    unsigned int p_align;
} __attribute__((packed)) elf32_phdr_t;

#define ELFCLASS32 1
#define ELFDATA2LSB 1
#define ET_EXEC 2
#define EM_386  3
#define PT_LOAD 1

int elf32_load(const unsigned char* image, unsigned int image_size, unsigned int* out_entry)
{
    if (image == (const unsigned char*)0 || out_entry == (unsigned int*)0) {
        return 0;
    }

    if (image_size < sizeof(elf32_ehdr_t)) {
        return 0; // too small to even contain a header
    }

    const elf32_ehdr_t* eh = (const elf32_ehdr_t*)image;

    if (eh->e_ident[0] != 0x7F || eh->e_ident[1] != 'E' ||
        eh->e_ident[2] != 'L'  || eh->e_ident[3] != 'F') {
        return 0; // not an ELF file at all
    }

    if (eh->e_ident[4] != ELFCLASS32) {
        return 0; // not 32-bit
    }

    if (eh->e_ident[5] != ELFDATA2LSB) {
        return 0; // not little-endian
    }

    if (eh->e_type != ET_EXEC) {
        return 0; // only plain executables supported (no PIE/shared objects)
    }

    if (eh->e_machine != EM_386) {
        return 0; // not i386
    }

    if (eh->e_phnum == 0) {
        return 0; // nothing to load
    }

    // bounds-check the program header table itself against the image
    unsigned int phtable_size = (unsigned int)eh->e_phentsize * eh->e_phnum;
    if (eh->e_phoff > image_size || phtable_size > image_size - eh->e_phoff) {
        return 0; // program header table doesn't fit in the file
    }

    for (unsigned short i = 0; i < eh->e_phnum; i++) {
        const elf32_phdr_t* ph = (const elf32_phdr_t*)(image + eh->e_phoff + (unsigned int)i * eh->e_phentsize);

        if (ph->p_type != PT_LOAD) {
            continue;
        }

        // bounds-check this segment's source range against the image buffer
        if (ph->p_offset > image_size || ph->p_filesz > image_size - ph->p_offset) {
            return 0; // segment claims data past the end of the file - corrupt/malicious
        }

        if (ph->p_memsz < ph->p_filesz) {
            return 0; // nonsensical - memsz must be at least filesz
        }

        unsigned char* dest = (unsigned char*)ph->p_vaddr;
        const unsigned char* src = image + ph->p_offset;

        for (unsigned int b = 0; b < ph->p_filesz; b++) {
            dest[b] = src[b];
        }

        // zero the remainder (this is .bss within the segment - the
        // file doesn't store zero bytes, memsz just reserves room)
        for (unsigned int b = ph->p_filesz; b < ph->p_memsz; b++) {
            dest[b] = 0;
        }
    }

    *out_entry = eh->e_entry;
    return 1;
}

void elf32_run(unsigned int entry)
{
    void (*entry_fn)(void) = (void (*)(void))entry;
    entry_fn();
}

int elf32_load_and_run(const unsigned char* image, unsigned int image_size)
{
    unsigned int entry;
    if (!elf32_load(image, image_size, &entry)) {
        return 0;
    }
    elf32_run(entry);
    return 1;
}