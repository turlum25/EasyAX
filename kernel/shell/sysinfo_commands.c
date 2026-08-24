#include "headers/commands.h"
#include "headers/util.h"
#include "../headers/print.h"
#include "../syscalls/syscall.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../drivers/ata.h"
#include "../fs/mbr.h"
#include "../fs/fat16.h"
#include "../elf/elf32.h"

// command: uname
// options: -a (all), -s (kernel name, default), -r (release), -m (machine)
void command_uname(char* option)
{
    if (option == (char*)0 || strcmp(option, "-s") == 0) {
        print_text("AlderKernel\n");
        return;
    }

    if (strcmp(option, "-r") == 0) {
        print_text("0.0.5\n");
        return;
    }

    if (strcmp(option, "-m") == 0) {
        print_text("i386\n");
        return;
    }

    if (strcmp(option, "-a") == 0) {
        print_text("AlderKernel 0.0.5 i386 zSlash\n");
        return;
    }

    print_text("uname: unrecognized option: ");
    print_text(option);
    print_text("\n");
}

// command: req-syscallop <syscall number>
void command_req_syscallop(char* arg)
{
    if (arg == (char*)0) {
        print_text("Usage: req-syscallop <syscall number>\n");
        return;
    }

    unsigned int syscall_num = str_to_uint(arg);

    print_text("Syscall requested: #");
    print_uint(syscall_num);
    print_text("\n");

    syscall_dispatch(syscall_num);
}

// command: memtest
// NAIVE fixed-address memory test - there is no multiboot memory map
// check yet, so this blindly assumes the region below is free, usable
// RAM. That's true on typical QEMU defaults but is NOT a safe
// assumption on real hardware; replace with a real memory-map probe
// before trusting this outside a VM.
#define MEMTEST_BASE  ((volatile unsigned int*)0x400000) // 4MB mark
#define MEMTEST_WORDS 4096u                               // 16KB tested

void command_memtest(void)
{
    unsigned int pattern1 = 0xAAAAAAAA;
    unsigned int pattern2 = 0x55555555;
    unsigned int failures = 0;

    print_text("Running memory test at ");
    print_hex((unsigned int)MEMTEST_BASE);
    print_text(" (");
    print_uint(MEMTEST_WORDS * 4);
    print_text(" bytes)...\n");

    for (unsigned int i = 0; i < MEMTEST_WORDS; i++) {
        MEMTEST_BASE[i] = pattern1;
    }
    for (unsigned int i = 0; i < MEMTEST_WORDS; i++) {
        if (MEMTEST_BASE[i] != pattern1) {
            failures++;
        }
    }

    for (unsigned int i = 0; i < MEMTEST_WORDS; i++) {
        MEMTEST_BASE[i] = pattern2;
    }
    for (unsigned int i = 0; i < MEMTEST_WORDS; i++) {
        if (MEMTEST_BASE[i] != pattern2) {
            failures++;
        }
    }

    if (failures == 0) {
        print_text("Memory test PASSED.\n");
    } else {
        print_text("Memory test FAILED - ");
        print_uint(failures);
        print_text(" word mismatches.\n");
    }
}

// command: meminfo
void command_meminfo(void)
{
    print_text("Physical memory: ");
    print_uint(pmm_get_free_page_count());
    print_text(" / ");
    print_uint(pmm_get_total_page_count());
    print_text(" pages free (");
    print_uint(pmm_get_free_page_count() * 4);
    print_text(" KB)\n");

    print_text("Heap: ");
    print_uint(heap_get_free_bytes());
    print_text(" bytes free, ");
    print_uint(heap_get_used_bytes());
    print_text(" bytes used\n");
}

// command: diskinfo
void command_diskinfo(void)
{
    print_text("Probing primary ATA bus (master)...\n");

    if (!ata_identify()) {
        print_text("No drive detected.\n");
        return;
    }

    print_text("Drive detected. Reading sector 0 (MBR)...\n");

    unsigned char sector[512];
    if (!ata_read_sector(0, sector)) {
        print_text("Read failed.\n");
        return;
    }

    print_text("First 16 bytes: ");
    for (int i = 0; i < 16; i++) {
        print_hex(sector[i]);
        print_text(" ");
    }
    print_text("\n");

    print_text("Boot signature (should be 0xAA55 at offset 510): ");
    print_hex(sector[510] | (sector[511] << 8));
    print_text("\n");
}

// command: partinfo
void command_partinfo(void)
{
    mbr_partition_t parts[4];
    int used = mbr_read_partitions(parts);

    if (used == 0) {
        print_text("No valid MBR found (disk is unpartitioned).\n");
        return;
    }

    for (int i = 0; i < 4; i++) {
        if (parts[i].type == 0) {
            continue;
        }

        print_text("Partition ");
        print_uint(i);
        print_text(": type=");
        print_hex(parts[i].type);
        print_text(" bootable=");
        print_uint(parts[i].bootable);
        print_text(" start_lba=");
        print_uint(parts[i].lba_start);
        print_text(" sectors=");
        print_uint(parts[i].sector_count);
        print_text("\n");
    }
}

static int do_mkpart(unsigned int* out_start, unsigned int* out_count)
{
    if (!ata_identify()) {
        return 0;
    }

    unsigned int total = ata_get_total_sectors();
    if (total == 0) {
        return 0;
    }

    unsigned int start = 2048;
    if (total <= start) {
        return 0;
    }
    unsigned int count = total - start;

    if (!mbr_write_partition(0, 1, 0x06, start, count)) {
        return 0;
    }

    *out_start = start;
    *out_count = count;
    return 1;
}

static int do_mkfs(unsigned int start, unsigned int count)
{
    return fat16_format(start, count);
}

// command: mkpart
// writes a single FAT16-type partition covering the whole drive
// (minus a standard 1MB alignment gap at the start). DESTRUCTIVE -
// overwrites sector 0.
void command_mkpart(void)
{
    unsigned int start, count;
    if (!do_mkpart(&start, &count)) {
        print_text("Failed to write partition table (no drive, or drive too small).\n");
        return;
    }

    print_text("Partition table written: type=0x06 (FAT16) start_lba=");
    print_uint(start);
    print_text(" sectors=");
    print_uint(count);
    print_text("\n");
}

// command: mkfs
// formats the first FAT16-type partition found in the MBR. DESTRUCTIVE.
void command_mkfs(void)
{
    mbr_partition_t parts[4];
    if (mbr_read_partitions(parts) == 0) {
        print_text("No partition table found - run mkpart first.\n");
        return;
    }

    for (int i = 0; i < 4; i++) {
        if (parts[i].type != 0x06) {
            continue;
        }

        print_text("Formatting partition ");
        print_uint(i);
        print_text(" as FAT16 (");
        print_uint(parts[i].sector_count);
        print_text(" sectors)...\n");

        if (fat16_format(parts[i].lba_start, parts[i].sector_count)) {
            print_text("Format complete.\n");
        } else {
            print_text("Format failed (partition too small or invalid cluster count).\n");
        }
        return;
    }

    print_text("No FAT16 (type 0x06) partition found.\n");
}

static int install_placeholder_file(unsigned int dir_cluster, const char* name, const char* ext, const char* content)
{
    return fat16_create_file_in_dir(dir_cluster, name, ext, (const unsigned char*)content, str_len(content));
}

// command: install
// full one-shot install: partitions the disk, formats FAT16, and
// creates /apps /kernel /shell /system with placeholder files.
// DESTRUCTIVE - wipes whatever was on the disk before.
void command_install(void)
{
    print_text("=== AtomiXOS Installer (running on Alder Kernel) ===\n");
    print_text("WARNING: this ERASES the attached disk.\n");

    print_text("[1/4] Partitioning disk...\n");
    unsigned int part_start, part_count;
    if (!do_mkpart(&part_start, &part_count)) {
        print_text("Install failed: could not partition disk (no drive, or too small).\n");
        return;
    }
    print_text("      start_lba=");
    print_uint(part_start);
    print_text(" sectors=");
    print_uint(part_count);
    print_text("\n");

    print_text("[2/4] Formatting as FAT16...\n");
    if (!do_mkfs(part_start, part_count)) {
        print_text("Install failed: FAT16 format failed.\n");
        return;
    }

    print_text("[3/4] Mounting filesystem...\n");
    if (!fat16_mount(part_start)) {
        print_text("Install failed: mount failed right after formatting (unexpected).\n");
        return;
    }

    print_text("[4/4] Creating directory structure...\n");

    unsigned int apps_cl, kernel_cl, shell_cl, system_cl;

    if (!fat16_mkdir_root("APPS", "", &apps_cl)) {
        print_text("Failed to create /apps\n");
        return;
    }
    print_text("      /apps\n");

    if (!fat16_mkdir_root("KERNEL", "", &kernel_cl)) {
        print_text("Failed to create /kernel\n");
        return;
    }
    install_placeholder_file(kernel_cl, "KERNEL", "ELF", "Placeholder text file, not a real ELF32 binary. Use runelf to run a real one.\n");
    print_text("      /kernel/kernel.elf\n");

    if (!fat16_mkdir_root("SHELL", "", &shell_cl)) {
        print_text("Failed to create /shell\n");
        return;
    }
    install_placeholder_file(shell_cl, "ZSLASH", "ELF", "Placeholder text file, not a real ELF32 binary. Use runelf to run a real one.\n");
    print_text("      /shell/zslash.elf\n");

    if (!fat16_mkdir_root("SYSTEM", "", &system_cl)) {
        print_text("Failed to create /system\n");
        return;
    }
    install_placeholder_file(system_cl, "README", "TXT", "AtomiXOS FAT16 disk - installed via 'install'.\n");
    print_text("      /system/readme.txt\n");

    print_text("=== Install complete. ===\n");
    print_text("Drop real ELF32 binaries into /apps and run them with:\n");
    print_text("  runelf apps/<name>.elf\n");
    print_text("Next: on the HOST, run tools/install_to_disk.sh to install\n");
    print_text("GRUB and copy kernel.bin, making this disk directly bootable.\n");
}

static void print_dirent(fat16_dirent_t* d)
{
    if (d->attr & 0x10) {
        print_text("  DIR   ");
    } else {
        print_text("  FILE  ");
    }
    print_text(d->name);
    if (d->ext[0] != '\0') {
        print_text(".");
        print_text(d->ext);
    }
    if (!(d->attr & 0x10)) {
        print_text("  (");
        print_uint(d->size);
        print_text(" bytes)");
    }
    print_text("\n");
}

static int mount_fat16_partition(void)
{
    mbr_partition_t parts[4];
    if (mbr_read_partitions(parts) == 0) {
        return 0;
    }
    for (int i = 0; i < 4; i++) {
        if (parts[i].type == 0x06) {
            return fat16_mount(parts[i].lba_start);
        }
    }
    return 0;
}

// command: lsdisk [dirname]
void command_lsdisk(char* arg)
{
    if (!mount_fat16_partition()) {
        print_text("No FAT16 partition mounted - run mkpart/mkfs first.\n");
        return;
    }

    fat16_dirent_t entries[32];
    int count;

    if (arg == (char*)0) {
        count = fat16_list_root(entries, 32);
    } else {
        fat16_dirent_t dir;
        if (!fat16_find_in_root(arg, "", &dir) || !(dir.attr & 0x10)) {
            print_text("No such directory: ");
            print_text(arg);
            print_text("\n");
            return;
        }
        count = fat16_list_dir(dir.cluster, entries, 32);
    }

    if (count == 0) {
        print_text("(empty)\n");
        return;
    }

    int shown = count < 32 ? count : 32;
    for (int i = 0; i < shown; i++) {
        print_dirent(&entries[i]);
    }
}

// command: catdisk <file>  or  catdisk <dir>/<file>
void command_catdisk(char* arg)
{
    if (arg == (char*)0) {
        print_text("Usage: catdisk <file> or catdisk <dir>/<file>\n");
        return;
    }

    if (!mount_fat16_partition()) {
        print_text("No FAT16 partition mounted - run mkpart/mkfs first.\n");
        return;
    }

    int slash = -1;
    for (int i = 0; arg[i] != '\0'; i++) {
        if (arg[i] == '/') {
            slash = i;
            break;
        }
    }

    fat16_dirent_t file;
    int found = 0;

    if (slash < 0) {
        found = fat16_find_in_root(arg, "", &file);
        if (!found) {
            char name[9]; char ext[4];
            int dot = -1;
            for (int i = 0; arg[i] != '\0'; i++) if (arg[i] == '.') { dot = i; break; }
            if (dot >= 0) {
                int ni = 0;
                for (int i = 0; i < dot && ni < 8; i++) name[ni++] = arg[i];
                name[ni] = '\0';
                int ei = 0;
                for (int i = dot + 1; arg[i] != '\0' && ei < 3; i++) ext[ei++] = arg[i];
                ext[ei] = '\0';
                found = fat16_find_in_root(name, ext, &file);
            }
        }
    } else {
        arg[slash] = '\0';
        char* dirname = arg;
        char* rest = &arg[slash + 1];

        fat16_dirent_t dir;
        if (fat16_find_in_root(dirname, "", &dir) && (dir.attr & 0x10)) {
            char name[9]; char ext[4];
            int dot = -1;
            for (int i = 0; rest[i] != '\0'; i++) if (rest[i] == '.') { dot = i; break; }
            if (dot >= 0) {
                int ni = 0;
                for (int i = 0; i < dot && ni < 8; i++) name[ni++] = rest[i];
                name[ni] = '\0';
                int ei = 0;
                for (int i = dot + 1; rest[i] != '\0' && ei < 3; i++) ext[ei++] = rest[i];
                ext[ei] = '\0';
                found = fat16_find_in_dir(dir.cluster, name, ext, &file);
            } else {
                found = fat16_find_in_dir(dir.cluster, rest, "", &file);
            }
        }
        arg[slash] = '/';
    }

    if (!found) {
        print_text("File not found.\n");
        return;
    }

    if (file.size == 0) {
        print_text("(empty file)\n");
        return;
    }

    unsigned char* buf = (unsigned char*)kmalloc(file.size + 1);
    if (buf == (unsigned char*)0) {
        print_text("Out of heap memory.\n");
        return;
    }

    if (!fat16_read_file(file.cluster, file.size, buf, file.size)) {
        print_text("Read failed.\n");
        kfree(buf);
        return;
    }

    buf[file.size] = '\0';
    print_text((const char*)buf);
    print_text("\n");

    kfree(buf);
}
// command: runelf <path>
// loads and runs a trusted ELF32 binary from the FAT16 disk. RING 0,
// NO ISOLATION - see elf32.h for what this does and doesn't protect
// against.
void command_runelf(char* arg)
{
    if (arg == (char*)0) {
        print_text("Usage: runelf <file> or runelf <dir>/<file>\n");
        return;
    }

    if (!mount_fat16_partition()) {
        print_text("No FAT16 partition mounted - run mkpart/mkfs first.\n");
        return;
    }

    int slash = -1;
    for (int i = 0; arg[i] != '\0'; i++) {
        if (arg[i] == '/') {
            slash = i;
            break;
        }
    }

    fat16_dirent_t file;
    int found = 0;

    if (slash < 0) {
        char name[9]; char ext[4];
        int dot = -1;
        for (int i = 0; arg[i] != '\0'; i++) if (arg[i] == '.') { dot = i; break; }
        if (dot >= 0) {
            int ni = 0;
            for (int i = 0; i < dot && ni < 8; i++) name[ni++] = arg[i];
            name[ni] = '\0';
            int ei = 0;
            for (int i = dot + 1; arg[i] != '\0' && ei < 3; i++) ext[ei++] = arg[i];
            ext[ei] = '\0';
            found = fat16_find_in_root(name, ext, &file);
        } else {
            found = fat16_find_in_root(arg, "", &file);
        }
    } else {
        arg[slash] = '\0';
        fat16_dirent_t dir;
        if (fat16_find_in_root(arg, "", &dir) && (dir.attr & 0x10)) {
            char* rest = &arg[slash + 1];
            char name[9]; char ext[4];
            int dot = -1;
            for (int i = 0; rest[i] != '\0'; i++) if (rest[i] == '.') { dot = i; break; }
            if (dot >= 0) {
                int ni = 0;
                for (int i = 0; i < dot && ni < 8; i++) name[ni++] = rest[i];
                name[ni] = '\0';
                int ei = 0;
                for (int i = dot + 1; rest[i] != '\0' && ei < 3; i++) ext[ei++] = rest[i];
                ext[ei] = '\0';
                found = fat16_find_in_dir(dir.cluster, name, ext, &file);
            } else {
                found = fat16_find_in_dir(dir.cluster, rest, "", &file);
            }
        }
        arg[slash] = '/';
    }

    if (!found) {
        print_text("File not found.\n");
        return;
    }

    if (file.size == 0) {
        print_text("Empty file - not a valid ELF.\n");
        return;
    }

    unsigned char* buf = (unsigned char*)kmalloc(file.size);
    if (buf == (unsigned char*)0) {
        print_text("Out of heap memory.\n");
        return;
    }

    if (!fat16_read_file(file.cluster, file.size, buf, file.size)) {
        print_text("Read failed.\n");
        kfree(buf);
        return;
    }

    print_text("Loading ELF32 (");
    print_uint(file.size);
    print_text(" bytes)...\n");

    if (!elf32_load_and_run(buf, file.size)) {
        print_text("ELF load failed: invalid header, unsupported format, or corrupt program headers.\n");
        kfree(buf);
        return;
    }

    print_text("Program returned.\n");
    kfree(buf);
}
