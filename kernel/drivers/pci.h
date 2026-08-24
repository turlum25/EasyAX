#ifndef PCI_H
#define PCI_H

#define PCI_MAX_DEVICES 32 // plenty for a QEMU/real-hardware desktop board

typedef struct pci_device {
    unsigned char bus;
    unsigned char device;
    unsigned char function;

    unsigned short vendor_id;
    unsigned short device_id;

    unsigned char class_code;
    unsigned char subclass;
    unsigned char prog_if;
    unsigned char header_type;

    // raw BARs as read from config space (offsets 0x10-0x24), UNparsed -
    // callers that care about a specific BAR should use pci_get_bar_address()
    // rather than reading bars[] directly, since 64-bit BARs span two
    // consecutive entries and the low bits are type/flag bits, not address.
    unsigned int bars[6];
} pci_device_t;

// reads/writes raw PCI config space via the legacy 0xCF8/0xCFC mechanism.
// offset must be 4-byte aligned for the 32-bit variant.
unsigned int   pci_config_read32(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset);
unsigned short pci_config_read16(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset);
unsigned char  pci_config_read8(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset);
void           pci_config_write32(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset, unsigned int value);
void           pci_config_write16(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset, unsigned short value);

// brute-force scans bus 0-255 / device 0-31 / function 0-7 and records
// every present device. safe to call more than once - re-scans from
// scratch each time. returns the number of devices found.
unsigned int pci_scan(void);

// number of devices found by the most recent pci_scan()
unsigned int pci_get_device_count(void);

// returns device `index` from the most recent scan, or 0 if out of range
pci_device_t* pci_get_device(unsigned int index);

// convenience: finds the first device matching class/subclass/prog_if.
// pass 0xFF for any field you don't want to filter on.
// returns 0 if none found. must be called after pci_scan().
pci_device_t* pci_find_device(unsigned char class_code, unsigned char subclass, unsigned char prog_if);

// resolves BAR `bar_index` (0-5) to a physical address, transparently
// handling 64-bit BARs (which occupy two consecutive bar[] slots). Only
// meaningful for memory BARs - returns 0 for I/O-space BARs or an
// out-of-range index. If the 64-bit BAR's upper 32 bits are nonzero,
// the address doesn't fit in 32-bit and this returns 0 (we can't map
// it anyway on a 32-bit kernel).
unsigned int pci_get_bar_address(pci_device_t* dev, int bar_index);

// probes the size of memory BAR `bar_index` using the standard
// "write all-1s, read back" trick: temporarily writes 0xFFFFFFFF to
// the BAR register, reads back which address bits the hardware
// actually implements, then restores the original value. Returns the
// size in bytes, or 0 for an I/O-space BAR or out-of-range index.
// Only examines the low dword - if you also need this for a 64-bit
// BAR bigger than 4GB, this isn't it, but MMIO register windows like
// XHCI's are never anywhere close to that.
unsigned int pci_get_bar_size(pci_device_t* dev, int bar_index);

// enables memory-space access (bit 1) and bus mastering (bit 2) in the
// device's PCI command register - required before a device with a
// memory BAR (like XHCI) will respond to MMIO, and before it can DMA.
void pci_enable_device(pci_device_t* dev);

#endif