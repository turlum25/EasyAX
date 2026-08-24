#ifndef ATA_H
#define ATA_H

// reads/writes exactly 512 bytes (one sector) at LBA `lba`, primary
// bus, master drive. buffer must be at least 512 bytes.
// returns 1 on success, 0 on failure/timeout.
int ata_read_sector(unsigned int lba, unsigned char* buffer);
int ata_write_sector(unsigned int lba, const unsigned char* buffer);

unsigned int ata_get_total_sectors(void);

// returns 1 if a drive responded to IDENTIFY, 0 if none present
int ata_identify(void);

#endif
