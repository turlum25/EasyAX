#ifndef INITRAMFS_H
#define INITRAMFS_H

// Builds the initial in-RAM filesystem (mounts /, /kernel, /shell),
// then hands control over to kernel_main(). Never returns.
void initramfs_init(void);

#endif
