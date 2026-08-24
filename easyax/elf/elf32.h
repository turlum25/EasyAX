#ifndef ELF32_H
#define ELF32_H

// parses and loads an ELF32 executable already sitting in memory at
// `image` (e.g. read there via fat16_read_file). Copies each PT_LOAD
// segment to its p_vaddr (== physical address here, since AlderKernel
// currently identity-maps the first 128MB - there is no separate
// virtual address space yet).
//
// SAFETY: this performs NO isolation whatsoever. It runs in ring 0,
// on the kernel's own stack, with full hardware access, and does NOT
// check whether a segment's destination address overlaps the running
// kernel itself. A malicious or buggy ELF can corrupt or crash
// AlderKernel exactly as easily as a bug in kernel code could. This
// is a loader for TRUSTED code only, until ring 3 + a real user/kernel
// boundary exists.
//
// returns 1 on success (with *out_entry set to the entry point
// address) or 0 on failure (bad magic, unsupported class/machine,
// corrupt program headers, or a segment that doesn't fit in `image`).
int elf32_load(const unsigned char* image, unsigned int image_size, unsigned int* out_entry);

// jumps into an already-loaded entry point. Treats it as an ordinary
// C function call (`call` instruction) - for this to behave sanely,
// the loaded code must eventually `ret` normally (i.e. its _start
// should just be a plain C function that returns) rather than trying
// to halt/exit itself, since there is no process-exit mechanism yet.
void elf32_run(unsigned int entry);

// convenience: load then immediately run. Returns 0 without running
// anything if loading failed.
int elf32_load_and_run(const unsigned char* image, unsigned int image_size);

#endif