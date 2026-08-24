#include "syscall.h"
#include "../headers/print.h"
#include "../mm/heap.h"

static void sys_panic(void)
{
    print_text("\n*** KERNEL PANIC (syscall 0x10) ***\n");

    asm volatile("cli");
    while (1) {
        asm volatile("hlt");
    }
}

#define SYSCALL_TEST_ALLOC_SIZE 128

// deliberately does NOT kfree() what it allocates - the point is to
// let you watch meminfo's used-byte count actually move and stay
// moved, proving kmalloc really handed out real, persistent memory
// (not just returning a pointer that gets silently reclaimed).
static void sys_request_ram(void)
{
    void* ptr = kmalloc(SYSCALL_TEST_ALLOC_SIZE);

    if (ptr == (void*)0) {
        print_text("sys_request_ram: kmalloc failed - out of heap memory.\n");
        return;
    }

    print_text("sys_request_ram: allocated ");
    print_uint(SYSCALL_TEST_ALLOC_SIZE);
    print_text(" bytes at ");
    print_hex((unsigned int)ptr);
    print_text("\nheap now: ");
    print_uint(heap_get_free_bytes());
    print_text(" bytes free, ");
    print_uint(heap_get_used_bytes());
    print_text(" bytes used\n");
}

void syscall_dispatch(unsigned int num)
{
    switch (num) {
        case SYS_READ:
            print_text("sys_read: not implemented yet (no VFS-backed reads).\n");
            break;

        case SYS_WRITE:
            print_text("sys_write: not implemented yet (no VFS-backed writes).\n");
            break;

        case SYS_REQUEST_RAM:
            sys_request_ram();
            break;

        case SYS_UNKNOWN_03:
            print_text("syscall 0x03: reserved, not defined yet.\n");
            break;

        case SYS_KERNEL_PANIC:
            sys_panic();
            break;

        default:
            print_text("Unknown syscall number: ");
            print_uint(num);
            print_text("\n");
            break;
    }
}