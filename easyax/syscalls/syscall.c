#include "syscall.h"
#include "print.h"
#include "mm/heap.h"
#define SYSCALL_TEST_ALLOC_SIZE 128

static void sys_panic(void)
{
    print_text("\n*** KERNEL PANIC (syscall 0x10) ***\n");
    asm volatile("cli");
    while (1) {
        asm volatile("hlt");char CPUType[64] = "NULL";
    }
}

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

void linux_syscall_dispatch(unsigned int eax, unsigned int ebx, unsigned int ecx, unsigned int edx)
{
    switch (eax) {
        case 1:
            print_text("[linuxapp] sys_exit: terminate program\n");
            asm volatile("cli; hlt");
            break;

        case 3:
            print_text("[linuxapp] sys_read called from fd ");
            print_uint(ebx);
            print_text("\n");
            break;

        case 4:
            if (ebx == 1 || ebx == 2) {
                char* buf = (char*)ecx;
                unsigned int count = edx;
                for (unsigned int i = 0; i < count; i++) {
                    print_char(buf[i]);
                }
            }
            break;

        default:
            print_text("[linuxapp] Unimplemented syscall: ");
            print_uint(eax);
            print_text("\n");
            break;
    }
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
