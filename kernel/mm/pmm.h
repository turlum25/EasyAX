#ifndef PMM_H
#define PMM_H

void pmm_init(unsigned int multiboot_info_addr);

// returns the physical address of a free 4KB page, or 0 if out of memory
void* pmm_alloc_page(void);
void  pmm_free_page(void* addr);

unsigned int pmm_get_free_page_count(void);
unsigned int pmm_get_total_page_count(void);

#endif