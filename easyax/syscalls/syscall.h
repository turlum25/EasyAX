#ifndef SYSCALL_H
#define SYSCALL_H

// syscall numbers - see syscalls.md
#define SYS_READ           0x00 // TODO - needs VFS-backed file reads
#define SYS_WRITE          0x01 // TODO - needs VFS-backed file writes
#define SYS_REQUEST_RAM    0x02 // TODO - needs heap allocator
#define SYS_UNKNOWN_03     0x03 // reserved, not defined yet
#define SYS_KERNEL_PANIC   0x10

void syscall_dispatch(unsigned int num);

#endif