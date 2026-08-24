#include "ata.h"
#include "../headers/io.h"

// primary ATA bus, standard legacy I/O ports
#define ATA_DATA        0x1F0
#define ATA_ERROR       0x1F1
#define ATA_SECTOR_CNT  0x1F2
#define ATA_LBA_LO      0x1F3
#define ATA_LBA_MID     0x1F4
#define ATA_LBA_HI      0x1F5
#define ATA_DRIVE_HEAD  0x1F6
#define ATA_STATUS      0x1F7
#define ATA_COMMAND     0x1F7

#define ATA_CMD_READ    0x20
#define ATA_CMD_WRITE   0x30
#define ATA_CMD_IDENTIFY 0xEC

#define ATA_STATUS_ERR  0x01
#define ATA_STATUS_DRQ  0x08
#define ATA_STATUS_BSY  0x80

// polls until BSY clears - bails out after a bounded number of tries
// instead of looping forever if a drive is missing/wedged
static int ata_wait_ready(void)
{
    for (unsigned int i = 0; i < 100000; i++) {
        unsigned char status = inb(ATA_STATUS);
        if (!(status & ATA_STATUS_BSY)) {
            return (status & ATA_STATUS_ERR) ? 0 : 1;
        }
    }
    return 0; // timed out
}

static int ata_wait_drq(void)
{
    for (unsigned int i = 0; i < 100000; i++) {
        unsigned char status = inb(ATA_STATUS);
        if (status & ATA_STATUS_ERR) {
            return 0;
        }
        if (status & ATA_STATUS_DRQ) {
            return 1;
        }
    }
    return 0; // timed out
}

static unsigned int total_sectors_cache = 0;

int ata_identify(void)
{
    outb(ATA_DRIVE_HEAD, 0xA0); // master drive, LBA mode not needed for IDENTIFY
    outb(ATA_SECTOR_CNT, 0);
    outb(ATA_LBA_LO, 0);
    outb(ATA_LBA_MID, 0);
    outb(ATA_LBA_HI, 0);
    outb(ATA_COMMAND, ATA_CMD_IDENTIFY);

    unsigned char status = inb(ATA_STATUS);
    if (status == 0) {
        return 0; // no drive on this bus at all
    }

    if (!ata_wait_drq()) {
        return 0;
    }

    unsigned short words[256];
    for (int i = 0; i < 256; i++) {
        words[i] = inw(ATA_DATA);
    }

    // words 60-61: total user-addressable sectors (LBA28), low word first
    total_sectors_cache = (unsigned int)words[60] | ((unsigned int)words[61] << 16);

    return 1;
}

unsigned int ata_get_total_sectors(void)
{
    return total_sectors_cache;
}

int ata_read_sector(unsigned int lba, unsigned char* buffer)
{
    if (!ata_wait_ready()) {
        return 0;
    }

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F)); // LBA28, master
    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LO,  lba        & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8)  & 0xFF);
    outb(ATA_LBA_HI,  (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, ATA_CMD_READ);

    if (!ata_wait_drq()) {
        return 0;
    }

    unsigned short* buf16 = (unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        buf16[i] = inw(ATA_DATA);
    }

    return 1;
}

int ata_write_sector(unsigned int lba, const unsigned char* buffer)
{
    if (!ata_wait_ready()) {
        return 0;
    }

    outb(ATA_DRIVE_HEAD, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_CNT, 1);
    outb(ATA_LBA_LO,  lba        & 0xFF);
    outb(ATA_LBA_MID, (lba >> 8)  & 0xFF);
    outb(ATA_LBA_HI,  (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, ATA_CMD_WRITE);

    if (!ata_wait_drq()) {
        return 0;
    }

    const unsigned short* buf16 = (const unsigned short*)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA, buf16[i]);
    }

    return 1;
}
