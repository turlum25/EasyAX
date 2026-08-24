#ifndef MBR_H
#define MBR_H

typedef struct {
    unsigned char bootable;     // 1 if the bootable flag is set
    unsigned char type;         // partition type byte (0x06/0x0E = FAT16)
    unsigned int  lba_start;
    unsigned int  sector_count;
} mbr_partition_t;

// reads sector 0 and parses up to 4 partition table entries into
// out[4] (unused slots are left zeroed). returns the number of
// entries with a nonzero type, or 0 if the MBR signature (0xAA55)
// is missing/invalid - i.e. the disk has no partition table yet.
int mbr_read_partitions(mbr_partition_t out[4]);

int mbr_write_partition(int index, unsigned char bootable, unsigned char type, unsigned int lba_start, unsigned int sector_count);

#endif
