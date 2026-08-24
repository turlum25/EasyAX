#ifndef PAGING_H
#define PAGING_H

// identity-maps the first 128MB as before, PLUS a second region covering
// [extra_region_addr, extra_region_addr + extra_region_size) - used to
// map in the framebuffer, whose physical address is usually a PCI BAR
// well above 128MB and would otherwise page-fault the moment we tried
// to draw to it. Pass extra_region_size = 0 if there's nothing extra
// to map (e.g. framebuffer tag was missing).
void paging_init(unsigned int extra_region_addr, unsigned int extra_region_size);

// identity-maps [phys_addr, phys_addr + size) into the SAME page
// directory paging_init() set up, sharing its pool of spare page
// tables. Safe to call any time after paging_init() (including well
// after paging is enabled - new PRESENT entries take effect
// immediately, no CR3 reload needed) and safe to call more than once,
// even with overlapping ranges. Use this to map an MMIO BAR whose
// physical address is only known once PCI enumeration has run (e.g.
// XHCI), which paging_init() can't have mapped at boot.
//
// returns 1 if the whole region is mapped, 0 if we ran out of spare
// page tables - treat 0 as a hard failure (don't dereference the
// region; it's only partially, if at all, mapped).
int paging_map_region(unsigned int phys_addr, unsigned int size);

#endif