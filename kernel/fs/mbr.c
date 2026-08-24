#include "mbr.h"
#include "../drivers/ata.h"

#define MBR_PARTITION_TABLE_OFFSET 446
#define MBR_SIGNATURE_OFFSET       510
#define MBR_ENTRY_SIZE             16

int mbr_read_partitions(mbr_partition_t out[4])
{
    unsigned char sector[512];

    for (int i = 0; i < 4; i++) {
        out[i].bootable = 0;
        out[i].type = 0;
        out[i].lba_start = 0;
        out[i].sector_count = 0;
    }

    if (!ata_read_sector(0, sector)) {
        return 0;
    }

    unsigned int signature = sector[MBR_SIGNATURE_OFFSET] | (sector[MBR_SIGNATURE_OFFSET + 1] << 8);
    if (signature != 0xAA55) {
        return 0; // no valid MBR present yet
    }

    int used = 0;

    for (int i = 0; i < 4; i++) {
        unsigned char* entry = &sector[MBR_PARTITION_TABLE_OFFSET + i * MBR_ENTRY_SIZE];

        unsigned char status = entry[0];
        unsigned char type   = entry[4];
        unsigned int lba_start = (unsigned int)entry[8] | ((unsigned int)entry[9] << 8) |
                                  ((unsigned int)entry[10] << 16) | ((unsigned int)entry[11] << 24);
        unsigned int sector_count = (unsigned int)entry[12] | ((unsigned int)entry[13] << 8) |
                                     ((unsigned int)entry[14] << 16) | ((unsigned int)entry[15] << 24);

        out[i].bootable = (status == 0x80) ? 1 : 0;
        out[i].type = type;
        out[i].lba_start = lba_start;
        out[i].sector_count = sector_count;

        if (type != 0) {
            used++;
        }
    }

    return used;
}

int mbr_write_partition(int index, unsigned char bootable, unsigned char type, unsigned int lba_start, unsigned int sector_count)
{
    if (index < 0 || index > 3) {
        return 0;
    }

    unsigned char sector[512];

    if (!ata_read_sector(0, sector)) {
        return 0;
    }

    unsigned int signature = sector[MBR_SIGNATURE_OFFSET] | (sector[MBR_SIGNATURE_OFFSET + 1] << 8);
    if (signature != 0xAA55) {
        // no valid MBR yet - start from a clean sector rather than
        // trusting whatever garbage is currently on disk
        for (int i = 0; i < 512; i++) {
            sector[i] = 0;
        }
    }

    unsigned char* entry = &sector[MBR_PARTITION_TABLE_OFFSET + index * MBR_ENTRY_SIZE];

    entry[0] = bootable ? 0x80 : 0x00;
    entry[1] = 0; entry[2] = 0; entry[3] = 0; // CHS start - unused, LBA-only
    entry[4] = type;
    entry[5] = 0; entry[6] = 0; entry[7] = 0; // CHS end - unused

    entry[8]  = (unsigned char)(lba_start & 0xFF);
    entry[9]  = (unsigned char)((lba_start >> 8) & 0xFF);
    entry[10] = (unsigned char)((lba_start >> 16) & 0xFF);
    entry[11] = (unsigned char)((lba_start >> 24) & 0xFF);

    entry[12] = (unsigned char)(sector_count & 0xFF);
    entry[13] = (unsigned char)((sector_count >> 8) & 0xFF);
    entry[14] = (unsigned char)((sector_count >> 16) & 0xFF);
    entry[15] = (unsigned char)((sector_count >> 24) & 0xFF);

    sector[MBR_SIGNATURE_OFFSET]     = 0x55;
    sector[MBR_SIGNATURE_OFFSET + 1] = 0xAA;

    return ata_write_sector(0, sector);
}
