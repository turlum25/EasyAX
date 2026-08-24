#ifndef HEAP_H
#define HEAP_H

void heap_init(void);

void* kmalloc(unsigned int size);
void  kfree(void* ptr);

unsigned int heap_get_free_bytes(void);
unsigned int heap_get_used_bytes(void);

#endif