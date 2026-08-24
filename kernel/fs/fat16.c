#include "fat16.h"
#include "../drivers/ata.h"

#define BYTES_PER_SECTOR    512
#define SECTORS_PER_CLUSTER 64
#define RESERVED_SECTORS    1
#define NUM_FATS             2
#define ROOT_ENTRIES         512
#define ROOT_DIR_SECTORS    ((ROOT_ENTRIES * 32) / BYTES_PER_SECTOR)
#define FAT_SIZE_SECTORS    256

static void zero_sector(unsigned char* buf)
{
    for (int i = 0; i < BYTES_PER_SECTOR; i++) {
        buf[i] = 0;
    }
}

int fat16_format(unsigned int partition_lba, unsigned int sector_count)
{
    unsigned int non_data_sectors = RESERVED_SECTORS + (NUM_FATS * FAT_SIZE_SECTORS) + ROOT_DIR_SECTORS;

    if (sector_count <= non_data_sectors) {
        return 0;
    }

    unsigned int data_sectors = sector_count - non_data_sectors;
    unsigned int cluster_count = data_sectors / SECTORS_PER_CLUSTER;

    if (cluster_count < 4085 || cluster_count > 65524) {
        return 0;
    }

    unsigned char sector[BYTES_PER_SECTOR];

    zero_sector(sector);

    sector[0] = 0xEB; sector[1] = 0x3C; sector[2] = 0x90;

    const char oem[8] = {'A','L','D','E','R','K','R','N'};
    for (int i = 0; i < 8; i++) sector[3 + i] = (unsigned char)oem[i];

    sector[11] = (unsigned char)(BYTES_PER_SECTOR & 0xFF);
    sector[12] = (unsigned char)((BYTES_PER_SECTOR >> 8) & 0xFF);
    sector[13] = SECTORS_PER_CLUSTER;
    sector[14] = (unsigned char)(RESERVED_SECTORS & 0xFF);
    sector[15] = (unsigned char)((RESERVED_SECTORS >> 8) & 0xFF);
    sector[16] = NUM_FATS;
    sector[17] = (unsigned char)(ROOT_ENTRIES & 0xFF);
    sector[18] = (unsigned char)((ROOT_ENTRIES >> 8) & 0xFF);
    sector[19] = 0; sector[20] = 0;
    sector[21] = 0xF8;
    sector[22] = (unsigned char)(FAT_SIZE_SECTORS & 0xFF);
    sector[23] = (unsigned char)((FAT_SIZE_SECTORS >> 8) & 0xFF);
    sector[24] = 63; sector[25] = 0;
    sector[26] = 255; sector[27] = 0;

    sector[28] = (unsigned char)(partition_lba & 0xFF);
    sector[29] = (unsigned char)((partition_lba >> 8) & 0xFF);
    sector[30] = (unsigned char)((partition_lba >> 16) & 0xFF);
    sector[31] = (unsigned char)((partition_lba >> 24) & 0xFF);

    sector[32] = (unsigned char)(sector_count & 0xFF);
    sector[33] = (unsigned char)((sector_count >> 8) & 0xFF);
    sector[34] = (unsigned char)((sector_count >> 16) & 0xFF);
    sector[35] = (unsigned char)((sector_count >> 24) & 0xFF);

    sector[36] = 0x80;
    sector[37] = 0;
    sector[38] = 0x29;

    sector[39] = 0x78; sector[40] = 0x56; sector[41] = 0x34; sector[42] = 0x12;

    const char label[11] = {'A','L','D','E','R','K','R','N','L',' ',' '};
    for (int i = 0; i < 11; i++) sector[43 + i] = (unsigned char)label[i];

    const char fstype[8] = {'F','A','T','1','6',' ',' ',' '};
    for (int i = 0; i < 8; i++) sector[54 + i] = (unsigned char)fstype[i];

    sector[510] = 0x55;
    sector[511] = 0xAA;

    if (!ata_write_sector(partition_lba, sector)) {
        return 0;
    }

    unsigned int fat_start = partition_lba + RESERVED_SECTORS;

    for (unsigned int copy = 0; copy < NUM_FATS; copy++) {
        unsigned int base = fat_start + copy * FAT_SIZE_SECTORS;

        zero_sector(sector);
        sector[0] = 0xF8; sector[1] = 0xFF;
        sector[2] = 0xFF; sector[3] = 0xFF;

        if (!ata_write_sector(base, sector)) {
            return 0;
        }

        zero_sector(sector);
        for (unsigned int i = 1; i < FAT_SIZE_SECTORS; i++) {
            if (!ata_write_sector(base + i, sector)) {
                return 0;
            }
        }
    }

    unsigned int root_start = fat_start + (NUM_FATS * FAT_SIZE_SECTORS);
    zero_sector(sector);
    for (unsigned int i = 0; i < ROOT_DIR_SECTORS; i++) {
        if (!ata_write_sector(root_start + i, sector)) {
            return 0;
        }
    }

    return 1;
}

// ---------------------------------------------------------------
// mounted-volume state and read/write support
// ---------------------------------------------------------------

typedef struct {
    unsigned int partition_lba;
    unsigned int bytes_per_sector;
    unsigned int sectors_per_cluster;
    unsigned int reserved_sectors;
    unsigned int num_fats;
    unsigned int root_entries;
    unsigned int fat_size_sectors;
    unsigned int root_dir_sectors;
    unsigned int fat_start;
    unsigned int root_start;
    unsigned int data_start;
} fat16_volume_t;

static fat16_volume_t vol;
static int mounted = 0;

int fat16_mount(unsigned int partition_lba)
{
    unsigned char sector[BYTES_PER_SECTOR];

    if (!ata_read_sector(partition_lba, sector)) {
        return 0;
    }

    if (sector[510] != 0x55 || sector[511] != 0xAA) {
        return 0; // not a valid boot sector
    }

    vol.partition_lba = partition_lba;
    vol.bytes_per_sector = (unsigned int)sector[11] | ((unsigned int)sector[12] << 8);
    vol.sectors_per_cluster = sector[13];
    vol.reserved_sectors = (unsigned int)sector[14] | ((unsigned int)sector[15] << 8);
    vol.num_fats = sector[16];
    vol.root_entries = (unsigned int)sector[17] | ((unsigned int)sector[18] << 8);
    vol.fat_size_sectors = (unsigned int)sector[22] | ((unsigned int)sector[23] << 8);

    if (vol.bytes_per_sector == 0 || vol.sectors_per_cluster == 0) {
        return 0; // clearly not a real FAT16 boot sector
    }

    vol.root_dir_sectors = (vol.root_entries * 32 + vol.bytes_per_sector - 1) / vol.bytes_per_sector;
    vol.fat_start  = partition_lba + vol.reserved_sectors;
    vol.root_start = vol.fat_start + (vol.num_fats * vol.fat_size_sectors);
    vol.data_start = vol.root_start + vol.root_dir_sectors;

    mounted = 1;
    return 1;
}

static unsigned int cluster_to_lba(unsigned int cluster)
{
    return vol.data_start + (cluster - 2) * vol.sectors_per_cluster;
}

static unsigned short fat_read_entry(unsigned int cluster)
{
    unsigned int byte_offset = cluster * 2;
    unsigned int sector_index = byte_offset / vol.bytes_per_sector;
    unsigned int offset_in_sector = byte_offset % vol.bytes_per_sector;

    unsigned char sector[BYTES_PER_SECTOR];
    ata_read_sector(vol.fat_start + sector_index, sector);

    return (unsigned short)(sector[offset_in_sector] | (sector[offset_in_sector + 1] << 8));
}

static void fat_write_entry(unsigned int cluster, unsigned short value)
{
    unsigned int byte_offset = cluster * 2;
    unsigned int sector_index = byte_offset / vol.bytes_per_sector;
    unsigned int offset_in_sector = byte_offset % vol.bytes_per_sector;

    // write to BOTH FAT copies, so they stay in sync
    for (unsigned int copy = 0; copy < vol.num_fats; copy++) {
        unsigned int base = vol.fat_start + copy * vol.fat_size_sectors;
        unsigned char sector[BYTES_PER_SECTOR];
        ata_read_sector(base + sector_index, sector);
        sector[offset_in_sector] = (unsigned char)(value & 0xFF);
        sector[offset_in_sector + 1] = (unsigned char)((value >> 8) & 0xFF);
        ata_write_sector(base + sector_index, sector);
    }
}

// marks the returned cluster as end-of-chain immediately - callers
// that need a longer chain relink it via fat_write_entry as they go
static unsigned int fat_alloc_cluster(void)
{
    unsigned int max_cluster = (vol.fat_size_sectors * vol.bytes_per_sector) / 2;

    for (unsigned int c = 2; c < max_cluster; c++) {
        if (fat_read_entry(c) == 0x0000) {
            fat_write_entry(c, 0xFFFF);
            return c;
        }
    }
    return 0; // no free clusters
}

// copies src into dest, truncating/space-padding to exactly `len`
// bytes. never reads src past its '\0', so short strings are safe.
static void copy_padded(unsigned char* dest, const char* src, int len)
{
    int i = 0;
    while (i < len && src[i] != '\0') {
        dest[i] = (unsigned char)src[i];
        i++;
    }
    while (i < len) {
        dest[i] = ' ';
        i++;
    }
}

static void set_dir_entry(unsigned char* e, const char* name, const char* ext, unsigned char attr, unsigned int cluster, unsigned int size)
{
    copy_padded(e, name, 8);
    copy_padded(e + 8, ext, 3);
    e[11] = attr;

    for (int i = 12; i < 26; i++) {
        e[i] = 0; // reserved / create-time fields - not tracked yet
    }

    e[26] = (unsigned char)(cluster & 0xFF);
    e[27] = (unsigned char)((cluster >> 8) & 0xFF);
    e[28] = (unsigned char)(size & 0xFF);
    e[29] = (unsigned char)((size >> 8) & 0xFF);
    e[30] = (unsigned char)((size >> 16) & 0xFF);
    e[31] = (unsigned char)((size >> 24) & 0xFF);
}

static int write_entry_in_root(const unsigned char* entry32)
{
    unsigned char sector[BYTES_PER_SECTOR];

    for (unsigned int s = 0; s < vol.root_dir_sectors; s++) {
        unsigned int lba = vol.root_start + s;
        if (!ata_read_sector(lba, sector)) {
            return 0;
        }

        for (int i = 0; i < 16; i++) {
            unsigned char* e = &sector[i * 32];
            if (e[0] == 0x00 || e[0] == 0xE5) { // free or deleted slot
                for (int b = 0; b < 32; b++) {
                    e[b] = entry32[b];
                }
                return ata_write_sector(lba, sector);
            }
        }
    }

    return 0; // root directory full
}

static int write_entry_in_dir_cluster(unsigned int dir_cluster, const unsigned char* entry32)
{
    unsigned int base = cluster_to_lba(dir_cluster);
    unsigned char sector[BYTES_PER_SECTOR];

    for (unsigned int s = 0; s < vol.sectors_per_cluster; s++) {
        unsigned int lba = base + s;
        if (!ata_read_sector(lba, sector)) {
            return 0;
        }

        for (int i = 0; i < 16; i++) {
            unsigned char* e = &sector[i * 32];
            if (e[0] == 0x00 || e[0] == 0xE5) {
                for (int b = 0; b < 32; b++) {
                    e[b] = entry32[b];
                }
                return ata_write_sector(lba, sector);
            }
        }
    }

    return 0; // directory's first cluster is full
}

int fat16_mkdir_root(const char* name, const char* ext, unsigned int* out_cluster)
{
    if (!mounted) {
        return 0;
    }

    unsigned int cluster = fat_alloc_cluster();
    if (cluster == 0) {
        return 0;
    }

    unsigned char sector[BYTES_PER_SECTOR];
    zero_sector(sector);

    unsigned char dot_entry[32];
    set_dir_entry(dot_entry, ".", "", 0x10, cluster, 0);

    unsigned char dotdot_entry[32];
    set_dir_entry(dotdot_entry, "..", "", 0x10, 0, 0); // 0 = parent is root

    for (int b = 0; b < 32; b++) sector[b] = dot_entry[b];
    for (int b = 0; b < 32; b++) sector[32 + b] = dotdot_entry[b];

    unsigned int base = cluster_to_lba(cluster);
    if (!ata_write_sector(base, sector)) {
        return 0;
    }

    zero_sector(sector);
    for (unsigned int s = 1; s < vol.sectors_per_cluster; s++) {
        if (!ata_write_sector(base + s, sector)) {
            return 0;
        }
    }

    unsigned char root_entry[32];
    set_dir_entry(root_entry, name, ext, 0x10, cluster, 0);
    if (!write_entry_in_root(root_entry)) {
        return 0;
    }

    if (out_cluster != (unsigned int*)0) {
        *out_cluster = cluster;
    }

    return 1;
}

static int write_file_data(unsigned int first_cluster, const unsigned char* data, unsigned int size)
{
    unsigned int bytes_per_cluster = vol.sectors_per_cluster * vol.bytes_per_sector;
    unsigned int cluster = first_cluster;
    unsigned int offset = 0;
    unsigned int remaining = size;

    while (1) {
        unsigned int base = cluster_to_lba(cluster);
        unsigned int this_chunk = (remaining < bytes_per_cluster) ? remaining : bytes_per_cluster;
        unsigned int written_in_cluster = 0;

        unsigned char sector[BYTES_PER_SECTOR];
        for (unsigned int s = 0; s < vol.sectors_per_cluster; s++) {
            zero_sector(sector);

            if (written_in_cluster < this_chunk) {
                unsigned int copy_len = (this_chunk - written_in_cluster) < BYTES_PER_SECTOR
                                         ? (this_chunk - written_in_cluster) : BYTES_PER_SECTOR;
                for (unsigned int b = 0; b < copy_len; b++) {
                    sector[b] = data[offset + written_in_cluster + b];
                }
            }

            if (!ata_write_sector(base + s, sector)) {
                return 0;
            }
            written_in_cluster += BYTES_PER_SECTOR;
        }

        offset += this_chunk;
        remaining -= this_chunk;

        if (remaining == 0) {
            fat_write_entry(cluster, 0xFFFF);
            break;
        }

        unsigned int next = fat_alloc_cluster();
        if (next == 0) {
            return 0; // ran out of space mid-write
        }
        fat_write_entry(cluster, next);
        cluster = next;
    }

    return 1;
}

int fat16_create_file_root(const char* name, const char* ext, const unsigned char* data, unsigned int size)
{
    if (!mounted) {
        return 0;
    }

    unsigned int first_cluster = 0;

    if (size > 0) {
        first_cluster = fat_alloc_cluster();
        if (first_cluster == 0) {
            return 0;
        }
        if (!write_file_data(first_cluster, data, size)) {
            return 0;
        }
    }

    unsigned char entry[32];
    set_dir_entry(entry, name, ext, 0x20, first_cluster, size);
    return write_entry_in_root(entry);
}

int fat16_create_file_in_dir(unsigned int dir_cluster, const char* name, const char* ext, const unsigned char* data, unsigned int size)
{
    if (!mounted) {
        return 0;
    }

    unsigned int first_cluster = 0;

    if (size > 0) {
        first_cluster = fat_alloc_cluster();
        if (first_cluster == 0) {
            return 0;
        }
        if (!write_file_data(first_cluster, data, size)) {
            return 0;
        }
    }

    unsigned char entry[32];
    set_dir_entry(entry, name, ext, 0x20, first_cluster, size);
    return write_entry_in_dir_cluster(dir_cluster, entry);
}
// ---------------------------------------------------------------
// read support
// ---------------------------------------------------------------

// case-insensitive comparison against a raw 11-byte on-disk name field
static int names_equal(const unsigned char* entry11, const char* name, const char* ext)
{
    unsigned char qname[8];
    unsigned char qext[3];
    copy_padded(qname, name, 8);
    copy_padded(qext, ext, 3);

    for (int i = 0; i < 8; i++) {
        unsigned char c = qname[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 32);
        if (entry11[i] != c) return 0;
    }
    for (int i = 0; i < 3; i++) {
        unsigned char c = qext[i];
        if (c >= 'a' && c <= 'z') c = (unsigned char)(c - 32);
        if (entry11[8 + i] != c) return 0;
    }
    return 1;
}

static void trim_copy(char* dest, const unsigned char* src, int len)
{
    int end = len;
    while (end > 0 && src[end - 1] == ' ') {
        end--;
    }
    for (int i = 0; i < end; i++) {
        dest[i] = (char)src[i];
    }
    dest[end] = '\0';
}

static void fill_dirent(fat16_dirent_t* d, const unsigned char* e)
{
    trim_copy(d->name, e, 8);
    trim_copy(d->ext, e + 8, 3);
    d->attr = e[11];
    d->cluster = (unsigned int)e[26] | ((unsigned int)e[27] << 8);
    d->size = (unsigned int)e[28] | ((unsigned int)e[29] << 8) |
              ((unsigned int)e[30] << 16) | ((unsigned int)e[31] << 24);
}

static int list_dir_sectors(unsigned int start_lba, unsigned int num_sectors, fat16_dirent_t* out, int max_entries)
{
    int count = 0;
    unsigned char sector[BYTES_PER_SECTOR];

    for (unsigned int s = 0; s < num_sectors; s++) {
        if (!ata_read_sector(start_lba + s, sector)) {
            return count;
        }

        for (int i = 0; i < 16; i++) {
            unsigned char* e = &sector[i * 32];

            if (e[0] == 0x00) {
                return count;
            }
            if (e[0] == 0xE5 || e[11] == 0x0F) {
                continue;
            }

            if (count < max_entries) {
                fill_dirent(&out[count], e);
            }
            count++;
        }
    }

    return count;
}

int fat16_list_root(fat16_dirent_t* out, int max_entries)
{
    if (!mounted) {
        return -1;
    }
    return list_dir_sectors(vol.root_start, vol.root_dir_sectors, out, max_entries);
}

int fat16_list_dir(unsigned int dir_cluster, fat16_dirent_t* out, int max_entries)
{
    if (!mounted) {
        return -1;
    }
    return list_dir_sectors(cluster_to_lba(dir_cluster), vol.sectors_per_cluster, out, max_entries);
}

static int find_in_sectors(unsigned int start_lba, unsigned int num_sectors, const char* name, const char* ext, fat16_dirent_t* out)
{
    unsigned char sector[BYTES_PER_SECTOR];

    for (unsigned int s = 0; s < num_sectors; s++) {
        if (!ata_read_sector(start_lba + s, sector)) {
            return 0;
        }

        for (int i = 0; i < 16; i++) {
            unsigned char* e = &sector[i * 32];

            if (e[0] == 0x00) {
                return 0;
            }
            if (e[0] == 0xE5 || e[11] == 0x0F) {
                continue;
            }
            if (names_equal(e, name, ext)) {
                fill_dirent(out, e);
                return 1;
            }
        }
    }

    return 0;
}

int fat16_find_in_root(const char* name, const char* ext, fat16_dirent_t* out)
{
    if (!mounted) {
        return 0;
    }
    return find_in_sectors(vol.root_start, vol.root_dir_sectors, name, ext, out);
}

int fat16_find_in_dir(unsigned int dir_cluster, const char* name, const char* ext, fat16_dirent_t* out)
{
    if (!mounted) {
        return 0;
    }
    return find_in_sectors(cluster_to_lba(dir_cluster), vol.sectors_per_cluster, name, ext, out);
}

int fat16_read_file(unsigned int first_cluster, unsigned int size, unsigned char* buffer, unsigned int buffer_size)
{
    if (!mounted) {
        return 0;
    }
    if (size == 0 || first_cluster == 0) {
        return 1;
    }

    unsigned int bytes_per_cluster = vol.sectors_per_cluster * vol.bytes_per_sector;
    unsigned int cluster = first_cluster;
    unsigned int remaining = size;
    unsigned int offset = 0;

    unsigned char sector[BYTES_PER_SECTOR];

    while (remaining > 0) {
        unsigned int base = cluster_to_lba(cluster);
        unsigned int this_chunk = (remaining < bytes_per_cluster) ? remaining : bytes_per_cluster;
        unsigned int read_in_cluster = 0;

        for (unsigned int s = 0; s < vol.sectors_per_cluster && read_in_cluster < this_chunk; s++) {
            if (!ata_read_sector(base + s, sector)) {
                return 0;
            }

            unsigned int copy_len = (this_chunk - read_in_cluster) < BYTES_PER_SECTOR
                                     ? (this_chunk - read_in_cluster) : BYTES_PER_SECTOR;

            for (unsigned int b = 0; b < copy_len; b++) {
                unsigned int dest_index = offset + read_in_cluster + b;
                if (dest_index < buffer_size) {
                    buffer[dest_index] = sector[b];
                }
            }
            read_in_cluster += BYTES_PER_SECTOR;
        }

        offset += this_chunk;
        remaining -= this_chunk;

        if (remaining == 0) {
            break;
        }

        unsigned short next = fat_read_entry(cluster);
        if (next >= 0xFFF8) {
            break;
        }
        cluster = next;
    }

    return 1;
}
