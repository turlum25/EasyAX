#include "initramfs.h"
#include "fs/vfs.h"
#include "headers/print.h"

// defined in main.c - this is where control actually lands after
// early setup + the VFS are ready.
extern void kernel_main(void);

void initramfs_init(void)
{
    print_text("initramfs: mounting in-RAM filesystem...\n");

    vfs_init();

    print_text("initramfs: /, /kernel, /shell mounted.\n");
    print_text("initramfs: handing control to kernel...\n");

    kernel_main(); // never returns
}
