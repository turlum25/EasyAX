# object files
OBJ_DIR = objects
OBJ = $(addprefix $(OBJ_DIR)/, boot.o main.o print.o idt_asm.o idt_c-code.o io.o ps2_driver.o shell.o interpreter.o pic_driver.o util.o sysinfo_commands.o vfs.o initramfs.o fs_commands.o syscall.o pmm.o heap.o paging.o ata_driver.o mbr.o fat16.o elf32.o pci_driver.o)
# flags and path to headers folder
CFLAGS = -m32 -ffreestanding -O0 -fno-pic -fno-pie -fno-stack-protector -Ikernel/headers -Ikernel -Ieasyax -Ikernel/drivers/cpu -Ikernel/drivers/sleep -mno-sse -mno-sse2 -mno-mmx -msoft-float -c

all: easyax.iso

# rule to ensure objects directory exists
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(OBJ_DIR)/boot.o: boot.asm | $(OBJ_DIR)
	# assemble bootloader to elf32 for grub
	nasm -f elf32 boot.asm -o $(OBJ_DIR)/boot.o
	
$(OBJ_DIR)/main.o: kernel/main.c | $(OBJ_DIR)
	# compile main kernel code
	gcc $(CFLAGS) kernel/main.c -o $(OBJ_DIR)/main.o

$(OBJ_DIR)/print.o: kernel/print.c | $(OBJ_DIR)
	# compile printing and screen functions
	gcc $(CFLAGS) kernel/print.c -o $(OBJ_DIR)/print.o

$(OBJ_DIR)/idt_asm.o: kernel/idt/idt_asm.asm | $(OBJ_DIR)
	# compile interrupt descriptor table
	nasm -f elf32 kernel/idt/idt_asm.asm -o $(OBJ_DIR)/idt_asm.o

$(OBJ_DIR)/idt_c-code.o: kernel/idt/idt.c | $(OBJ_DIR)
	# compile c version of idt
	gcc $(CFLAGS) kernel/idt/idt.c -o $(OBJ_DIR)/idt_c-code.o

$(OBJ_DIR)/io.o: kernel/io.c | $(OBJ_DIR)
	# compile hardware port input/output fn
	gcc $(CFLAGS) kernel/io.c -o $(OBJ_DIR)/io.o

$(OBJ_DIR)/ps2_driver.o: kernel/drivers/ps2.c | $(OBJ_DIR)
	# compile ps2 driver
	gcc $(CFLAGS) kernel/drivers/ps2.c -o $(OBJ_DIR)/ps2_driver.o

$(OBJ_DIR)/shell.o: easyax/shell/shell.c | $(OBJ_DIR)
	# compile zSlash Shell
	gcc $(CFLAGS) easyax/shell/shell.c -o $(OBJ_DIR)/shell.o

$(OBJ_DIR)/interpreter.o: easyax/shell/interpreter.c | $(OBJ_DIR)
	# compile Shell command interpreter
	gcc $(CFLAGS) easyax/shell/interpreter.c -o $(OBJ_DIR)/interpreter.o

$(OBJ_DIR)/pic_driver.o: kernel/interrupts/pic.c | $(OBJ_DIR)
	# compile pic driver
	gcc $(CFLAGS) kernel/interrupts/pic.c -o $(OBJ_DIR)/pic_driver.o

$(OBJ_DIR)/util.o: easyax/shell/util.c | $(OBJ_DIR)
	# compile shared shell string/number utilities
	gcc $(CFLAGS) easyax/shell/util.c -o $(OBJ_DIR)/util.o

$(OBJ_DIR)/sysinfo_commands.o: easyax/shell/sysinfo_commands.c | $(OBJ_DIR)
	# compile uname/req-syscallop/memtest commands
	gcc $(CFLAGS) easyax/shell/sysinfo_commands.c -o $(OBJ_DIR)/sysinfo_commands.o

$(OBJ_DIR)/vfs.o: easyax/fs/vfs.c | $(OBJ_DIR)
	# compile in-ram virtual file system
	gcc $(CFLAGS) easyax/fs/vfs.c -o $(OBJ_DIR)/vfs.o

$(OBJ_DIR)/initramfs.o: easyax/initramfs.c | $(OBJ_DIR)
	# compile initramfs: mounts the VFS, then hands control to kernel_main
	gcc $(CFLAGS) easyax/initramfs.c -o $(OBJ_DIR)/initramfs.o

$(OBJ_DIR)/fs_commands.o: easyax/shell/fs_commands.c | $(OBJ_DIR)
	# compile ls/cd/pwd/cat shell commands
	gcc $(CFLAGS) easyax/shell/fs_commands.c -o $(OBJ_DIR)/fs_commands.o

$(OBJ_DIR)/syscall.o: easyax/syscalls/syscall.c | $(OBJ_DIR)
	# compile syscall dispatcher
	gcc $(CFLAGS) easyax/syscalls/syscall.c -o $(OBJ_DIR)/syscall.o

$(OBJ_DIR)/pmm.o: kernel/mm/pmm.c | $(OBJ_DIR)
	# compile physical memory manager (bitmap allocator)
	gcc $(CFLAGS) kernel/mm/pmm.c -o $(OBJ_DIR)/pmm.o

$(OBJ_DIR)/heap.o: kernel/mm/heap.c | $(OBJ_DIR)
	# compile heap allocator (kmalloc/kfree)
	gcc $(CFLAGS) kernel/mm/heap.c -o $(OBJ_DIR)/heap.o

$(OBJ_DIR)/paging.o: kernel/mm/paging.c | $(OBJ_DIR)
	# compile paging (identity-mapped page tables)
	gcc $(CFLAGS) kernel/mm/paging.c -o $(OBJ_DIR)/paging.o

$(OBJ_DIR)/ata_driver.o: kernel/drivers/ata.c | $(OBJ_DIR)
	# compile ATA PIO disk driver
	gcc $(CFLAGS) kernel/drivers/ata.c -o $(OBJ_DIR)/ata_driver.o

$(OBJ_DIR)/pci_driver.o: kernel/drivers/pci.c | $(OBJ_DIR)
	# compile PCI config space access + bus enumeration
	gcc $(CFLAGS) kernel/drivers/pci.c -o $(OBJ_DIR)/pci_driver.o

$(OBJ_DIR)/mbr.o: easyax/fs/mbr.c | $(OBJ_DIR)
	# compile MBR partition table parser
	gcc $(CFLAGS) easyax/fs/mbr.c -o $(OBJ_DIR)/mbr.o

$(OBJ_DIR)/fat16.o: easyax/fs/fat16.c | $(OBJ_DIR)
	# compile FAT16 formatter
	gcc $(CFLAGS) easyax/fs/fat16.c -o $(OBJ_DIR)/fat16.o

$(OBJ_DIR)/elf32.o: easyax/elf/elf32.c | $(OBJ_DIR)
	# compile ELF32 loader (ring 0, no isolation)
	gcc $(CFLAGS) easyax/elf/elf32.c -o $(OBJ_DIR)/elf32.o

kernel.bin: $(OBJ)
	# link everything using linker script
	ld -m elf_i386 -T linker.ld --build-id=none $(OBJ) -o kernel.bin

easyax.iso: kernel.bin
	# build grub iso image
	mkdir -p iso/boot/grub
	cp kernel.bin iso/boot/
	cp grub.cfg iso/boot/grub/
	grub-mkrescue -o easyax.iso iso

clean:
	rm -rf $(OBJ_DIR) *.bin easyax.iso iso/

run: easyax.iso
	# run iso in qemu
	qemu-system-i386 -cdrom easyax.iso

run_wdisk: easyax.iso
	# run iso in qemu with a raw disk attached on the primary ATA bus
	qemu-system-i386 -cdrom easyax.iso -hda disk.img -boot d

deploy: easyax.iso
	# write the kernel into disk.img and turn on qemu with the disk.img
	./tools/install_to_disk.sh
	qemu-system-i386 -hda disk.img
