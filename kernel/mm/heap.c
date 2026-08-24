#include "heap.h"
#include "pmm.h"

#define PAGE_SIZE          4096
#define HEAP_INITIAL_PAGES 16   // 64KB initial heap
#define MIN_SPLIT_SIZE     16   // don't split off free slivers smaller than this
#define ALIGNMENT          8

typedef struct heap_block {
    unsigned int size;   // usable bytes AFTER this header (not counting the header)
    int free;
    struct heap_block* next;
    struct heap_block* prev;
} heap_block_t;

static heap_block_t* heap_head = (heap_block_t*)0;
static unsigned int used_bytes = 0;
static unsigned int free_bytes = 0;

static unsigned int align_up(unsigned int n, unsigned int align)
{
    return (n + align - 1) & ~(align - 1);
}

// grabs `pages` physical pages from the PMM and appends them to the
// heap as one new free block - but only for as long as the pages the
// PMM hands back are actually contiguous. If page N+1 isn't
// immediately after page N, we stop there and use what we got rather
// than assuming contiguity we don't actually have.
static heap_block_t* heap_extend(unsigned int pages)
{
    void* first = pmm_alloc_page();
    if (first == (void*)0) {
        return (heap_block_t*)0; // out of physical memory entirely
    }

    unsigned int got_pages = 1;
    unsigned char* region_start = (unsigned char*)first;
    unsigned char* expected_next = region_start + PAGE_SIZE;

    for (unsigned int i = 1; i < pages; i++) {
        void* page = pmm_alloc_page();
        if (page == (void*)0) {
            break; // out of memory - use what we have so far
        }
        if ((unsigned char*)page != expected_next) {
            pmm_free_page(page); // not contiguous - give it back, stop here
            break;
        }
        got_pages++;
        expected_next += PAGE_SIZE;
    }

    heap_block_t* block = (heap_block_t*)region_start;
    block->size = (got_pages * PAGE_SIZE) - sizeof(heap_block_t);
    block->free = 1;
    block->next = (heap_block_t*)0;
    block->prev = (heap_block_t*)0;

    free_bytes += block->size;

    // insert at the end of the address-ordered list. This is safe
    // because the PMM allocates low-to-high and never reuses a page
    // that's still in use, so heap_extend always gets a higher
    // address than anything already in the heap.
    if (heap_head == (heap_block_t*)0) {
        heap_head = block;
    } else {
        heap_block_t* tail = heap_head;
        while (tail->next != (heap_block_t*)0) {
            tail = tail->next;
        }
        tail->next = block;
        block->prev = tail;
    }

    return block;
}

void heap_init(void)
{
    heap_head = (heap_block_t*)0;
    used_bytes = 0;
    free_bytes = 0;

    heap_extend(HEAP_INITIAL_PAGES);
}

void* kmalloc(unsigned int size)
{
    if (size == 0) {
        return (void*)0;
    }

    size = align_up(size, ALIGNMENT);

    heap_block_t* block = heap_head;
    while (block != (heap_block_t*)0) {
        if (block->free && block->size >= size) {
            break;
        }
        block = block->next;
    }

    if (block == (heap_block_t*)0) {
        // nothing free is big enough - grow the heap and try once more
        unsigned int need_pages = (size + sizeof(heap_block_t) + PAGE_SIZE - 1) / PAGE_SIZE;
        if (need_pages < 1) {
            need_pages = 1;
        }

        block = heap_extend(need_pages);
        if (block == (heap_block_t*)0 || block->size < size) {
            return (void*)0; // truly out of memory
        }
    }

    // split off the leftover, if it's big enough to be worth its own header
    if (block->size >= size + sizeof(heap_block_t) + MIN_SPLIT_SIZE) {
        heap_block_t* remainder = (heap_block_t*)((unsigned char*)block + sizeof(heap_block_t) + size);
        remainder->size = block->size - size - sizeof(heap_block_t);
        remainder->free = 1;
        remainder->next = block->next;
        remainder->prev = block;
        if (block->next != (heap_block_t*)0) {
            block->next->prev = remainder;
        }
        block->next = remainder;
        block->size = size;
    }

    block->free = 0;
    free_bytes -= block->size;
    used_bytes += block->size;

    return (void*)((unsigned char*)block + sizeof(heap_block_t));
}

// merges `block` with one neighbor, but only if they're free AND
// actually physically adjacent - list order matching address order
// is an invariant we maintain, not something to assume blindly.
static void try_merge(heap_block_t* block, heap_block_t* neighbor, int neighbor_is_next)
{
    if (neighbor == (heap_block_t*)0 || !neighbor->free) {
        return;
    }

    heap_block_t* lower  = neighbor_is_next ? block : neighbor;
    heap_block_t* higher = neighbor_is_next ? neighbor : block;

    unsigned char* lower_end = (unsigned char*)lower + sizeof(heap_block_t) + lower->size;
    if (lower_end != (unsigned char*)higher) {
        return; // not actually adjacent in physical memory
    }

    lower->size += sizeof(heap_block_t) + higher->size;
    lower->next = higher->next;
    if (higher->next != (heap_block_t*)0) {
        higher->next->prev = lower;
    }
}

void kfree(void* ptr)
{
    if (ptr == (void*)0) {
        return;
    }

    heap_block_t* block = (heap_block_t*)((unsigned char*)ptr - sizeof(heap_block_t));

    block->free = 1;
    used_bytes -= block->size;
    free_bytes += block->size;

    try_merge(block, block->next, 1);
    try_merge(block, block->prev, 0);
}

unsigned int heap_get_free_bytes(void) { return free_bytes; }
unsigned int heap_get_used_bytes(void) { return used_bytes; }