#include "paging.h"

#define PAGE_SIZE          4096
#define ENTRIES_PER_TABLE  1024
#define TABLE_SPAN         (ENTRIES_PER_TABLE * PAGE_SIZE) // 4MB per table
#define IDENTITY_MAP_MB    128   // MUST match pmm.c's MAX_MANAGED_MEMORY
#define TABLES_NEEDED      (IDENTITY_MAP_MB / 4) // each table covers 4MB

// extra page tables reserved for mapping in anything outside the low
// 128MB: the framebuffer (mapped once at boot by paging_init) and,
// later, whatever MMIO BARs drivers discover via PCI and map on demand
// via paging_map_region (XHCI etc). 8 tables = up to 32MB, which
// comfortably covers a 1080p 32bpp framebuffer (~8MB, worst case 2
// tables if badly misaligned) with plenty left over for a handful of
// controller BARs, which are typically well under 1MB each.
#define EXTRA_TABLES 8

#define PAGE_PRESENT 0x1
#define PAGE_RW      0x2

static unsigned int page_directory[ENTRIES_PER_TABLE] __attribute__((aligned(4096)));
static unsigned int page_tables[TABLES_NEEDED][ENTRIES_PER_TABLE] __attribute__((aligned(4096)));
static unsigned int extra_page_tables[EXTRA_TABLES][ENTRIES_PER_TABLE] __attribute__((aligned(4096)));

// how many of extra_page_tables[] have been handed out so far. MUST
// persist across calls - paging_init()'s framebuffer mapping and any
// later paging_map_region() calls share this one pool, and if this
// reset to 0 on every call, a second call would silently reuse (and
// corrupt) a table an earlier call is still using.
static unsigned int next_extra_table = 0;

static void enable_paging(unsigned int page_directory_phys)
{
    asm volatile("mov %0, %%cr3" :: "r"(page_directory_phys) : "memory");

    unsigned int cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; // set the PG bit
    asm volatile("mov %0, %%cr0" :: "r"(cr0) : "memory");
}

// identity-maps every 4MB-aligned directory slot needed to cover
// [region_addr, region_addr + region_size), using the extra_page_tables
// pool. Slots already mapped - either by the low-128MB loop, or by an
// earlier call to this function - are left alone, which is what makes
// it safe to call more than once (including with overlapping ranges)
// without one call's mapping getting clobbered by another's.
//
// note: we only ever ADD new PRESENT entries here, never change an
// existing one, so there's no stale-translation risk that would need
// an invlpg/TLB flush - a CPU can't have cached a valid translation
// for an address that was never present before.
//
// returns 1 if the whole region ended up mapped, 0 if we ran out of
// spare tables (or directory space) partway through - callers should
// treat that as a hard failure, not something to retry.
static int map_extra_region(unsigned int region_addr, unsigned int region_size)
{
    if (region_size == 0) {
        return 1;
    }

    unsigned int start = region_addr & ~(TABLE_SPAN - 1);
    unsigned int end   = (region_addr + region_size + TABLE_SPAN - 1) & ~(TABLE_SPAN - 1);

    // end wraps to 0 if the region runs right up to the top of the
    // 32-bit address space - clamp instead of looping forever.
    if (end == 0) {
        end = 0xFFFFFFFFu - TABLE_SPAN + 1;
    }

    int ok = 1;

    for (unsigned int addr = start; addr < end; addr += TABLE_SPAN) {
        unsigned int dir_index = addr / TABLE_SPAN;

        if (dir_index < TABLES_NEEDED) {
            continue; // already covered by the low-128MB identity map
        }
        if (dir_index >= ENTRIES_PER_TABLE) {
            ok = 0; // address doesn't fit in a 32-bit page directory at all
            continue;
        }
        if (page_directory[dir_index] & PAGE_PRESENT) {
            continue; // already mapped by an earlier call - nothing to do
        }
        if (next_extra_table >= EXTRA_TABLES) {
            ok = 0; // out of spare tables
            continue;
        }

        unsigned int* table = extra_page_tables[next_extra_table++];
        for (unsigned int entry = 0; entry < ENTRIES_PER_TABLE; entry++) {
            unsigned int phys = addr + entry * PAGE_SIZE;
            table[entry] = phys | PAGE_PRESENT | PAGE_RW;
        }
        page_directory[dir_index] = ((unsigned int)table) | PAGE_PRESENT | PAGE_RW;
    }

    return ok;
}

void paging_init(unsigned int extra_region_addr, unsigned int extra_region_size)
{
    for (unsigned int table = 0; table < TABLES_NEEDED; table++) {
        for (unsigned int entry = 0; entry < ENTRIES_PER_TABLE; entry++) {
            unsigned int phys = (table * ENTRIES_PER_TABLE + entry) * PAGE_SIZE;
            page_tables[table][entry] = phys | PAGE_PRESENT | PAGE_RW;
        }
        page_directory[table] = ((unsigned int)&page_tables[table][0]) | PAGE_PRESENT | PAGE_RW;
    }

    for (unsigned int table = TABLES_NEEDED; table < ENTRIES_PER_TABLE; table++) {
        page_directory[table] = 0; // not present - touching this = page fault (vector 14)
    }

    map_extra_region(extra_region_addr, extra_region_size);

    enable_paging((unsigned int)&page_directory[0]);
}

int paging_map_region(unsigned int phys_addr, unsigned int size)
{
    return map_extra_region(phys_addr, size);
}