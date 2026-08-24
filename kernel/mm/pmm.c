#include "pmm.h"
#include "multiboot2.h"

#define PAGE_SHIFT 12               // 4096 = 2^12 - using shifts, not
#define PAGE_SIZE  (1u << PAGE_SHIFT) // division, avoids pulling in any
                                       // 64-bit libgcc division helpers
#define MAX_MANAGED_MEMORY (128u * 1024 * 1024) // only manage the first 128MB
#define MAX_PAGES  (MAX_MANAGED_MEMORY >> PAGE_SHIFT) // 32768
#define BITMAP_SIZE (MAX_PAGES / 8)                    // 4096 bytes

static unsigned char bitmap[BITMAP_SIZE];
static unsigned int free_pages_count = 0;

// provided by linker.ld - the address right after the kernel's own
// image (code+data+bss). We only care about its ADDRESS, not its value.
extern unsigned char _kernel_end;

static inline void bitmap_set(unsigned int page)   { bitmap[page / 8] |= (unsigned char)(1 << (page % 8)); }
static inline void bitmap_clear(unsigned int page) { bitmap[page / 8] &= (unsigned char)~(1 << (page % 8)); }
static inline int  bitmap_test(unsigned int page)  { return bitmap[page / 8] & (1 << (page % 8)); }

static void reserve_page(unsigned int page)
{
    if (page < MAX_PAGES && !bitmap_test(page)) {
        bitmap_set(page);
        free_pages_count--;
    }
}

// mmap_tag_addr is a pointer to the multiboot2 MB2_TAG_MMAP tag itself
// (found by main.c walking the tag list), or 0 if none was present.
void pmm_init(unsigned int mmap_tag_addr)
{
    // start fully reserved - we only free pages the memory map
    // explicitly tells us are usable RAM
    for (unsigned int i = 0; i < MAX_PAGES; i++) {
        bitmap_set(i);
    }
    free_pages_count = 0;

    if (mmap_tag_addr != 0) {
        mb2_tag_mmap_t* mmap_tag = (mb2_tag_mmap_t*)mmap_tag_addr;

        // entries are entry_size bytes apart (NOT sizeof(mb2_mmap_entry_t) -
        // the spec allows entry_size to grow in future revisions), and the
        // entry list runs until `size` bytes (including the 16-byte tag
        // header) of the tag have been consumed.
        unsigned char* ptr = (unsigned char*)mmap_tag_addr + 16;
        unsigned char* end = (unsigned char*)mmap_tag_addr + mmap_tag->size;

        while (ptr + sizeof(mb2_mmap_entry_t) <= end) {
            mb2_mmap_entry_t* entry = (mb2_mmap_entry_t*)ptr;

            if (entry->type == 1) { // usable RAM
                unsigned long long region_end = entry->base_addr + entry->length;
                if (region_end > MAX_MANAGED_MEMORY) {
                    region_end = MAX_MANAGED_MEMORY;
                }

                unsigned int first_page = (unsigned int)(entry->base_addr >> PAGE_SHIFT);
                unsigned int last_page  = (unsigned int)(region_end >> PAGE_SHIFT);

                for (unsigned int p = first_page; p < last_page && p < MAX_PAGES; p++) {
                    if (bitmap_test(p)) {
                        bitmap_clear(p);
                        free_pages_count++;
                    }
                }
            }

            ptr += mmap_tag->entry_size;
        }
    }

    // regardless of what the map reported free, ALWAYS reserve the
    // low 1MB (BIOS area, VGA memory, etc.) and everything our own
    // kernel image occupies - never hand these out as free pages
    unsigned int kernel_end_page = ((unsigned int)&_kernel_end >> PAGE_SHIFT) + 1;

    for (unsigned int p = 0; p < kernel_end_page && p < MAX_PAGES; p++) {
        reserve_page(p);
    }
}

void* pmm_alloc_page(void)
{
    for (unsigned int i = 0; i < MAX_PAGES; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages_count--;
            return (void*)(i << PAGE_SHIFT);
        }
    }
    return (void*)0; // out of memory
}

void pmm_free_page(void* addr)
{
    unsigned int page = ((unsigned int)addr) >> PAGE_SHIFT;
    if (page < MAX_PAGES && bitmap_test(page)) {
        bitmap_clear(page);
        free_pages_count++;
    }
}

unsigned int pmm_get_free_page_count(void)  { return free_pages_count; }
unsigned int pmm_get_total_page_count(void) { return MAX_PAGES; }