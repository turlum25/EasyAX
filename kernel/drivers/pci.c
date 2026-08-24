#include "pci.h"
#include "../headers/io.h"

#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA    0xCFC

#define PCI_VENDOR_NONE    0xFFFF // reads as this on any nonexistent function

static unsigned int pci_config_address(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset)
{
    return 0x80000000u
        | ((unsigned int)bus << 16)
        | ((unsigned int)(device & 0x1F) << 11)
        | ((unsigned int)(function & 0x07) << 8)
        | (offset & 0xFC); // low 2 bits are dword-index bits, must be 0 here
}

unsigned int pci_config_read32(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset)
{
    outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, device, function, offset));
    return inl(PCI_CONFIG_DATA);
}

unsigned short pci_config_read16(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset)
{
    unsigned int dword = pci_config_read32(bus, device, function, offset & 0xFC);
    unsigned int shift = (offset & 0x02) * 8;
    return (unsigned short)((dword >> shift) & 0xFFFF);
}

unsigned char pci_config_read8(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset)
{
    unsigned int dword = pci_config_read32(bus, device, function, offset & 0xFC);
    unsigned int shift = (offset & 0x03) * 8;
    return (unsigned char)((dword >> shift) & 0xFF);
}

void pci_config_write32(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset, unsigned int value)
{
    outl(PCI_CONFIG_ADDRESS, pci_config_address(bus, device, function, offset));
    outl(PCI_CONFIG_DATA, value);
}

// read-modify-write, since the config data port only does 32-bit access
void pci_config_write16(unsigned char bus, unsigned char device, unsigned char function, unsigned char offset, unsigned short value)
{
    unsigned int dword = pci_config_read32(bus, device, function, offset & 0xFC);
    unsigned int shift = (offset & 0x02) * 8;
    dword &= ~(0xFFFFu << shift);
    dword |= ((unsigned int)value << shift);
    pci_config_write32(bus, device, function, offset & 0xFC, dword);
}

static pci_device_t devices[PCI_MAX_DEVICES];
static unsigned int device_count = 0;

// records one function as a pci_device_t, if there's still room and it
// isn't already present. returns 1 if it added something.
static int pci_record_device(unsigned char bus, unsigned char device, unsigned char function)
{
    if (device_count >= PCI_MAX_DEVICES) {
        return 0;
    }

    unsigned short vendor_id = pci_config_read16(bus, device, function, 0x00);
    if (vendor_id == PCI_VENDOR_NONE) {
        return 0;
    }

    pci_device_t* dev = &devices[device_count];
    dev->bus = bus;
    dev->device = device;
    dev->function = function;
    dev->vendor_id = vendor_id;
    dev->device_id = pci_config_read16(bus, device, function, 0x02);

    unsigned int class_reg = pci_config_read32(bus, device, function, 0x08);
    dev->class_code = (unsigned char)((class_reg >> 24) & 0xFF);
    dev->subclass   = (unsigned char)((class_reg >> 16) & 0xFF);
    dev->prog_if    = (unsigned char)((class_reg >> 8) & 0xFF);

    dev->header_type = pci_config_read8(bus, device, function, 0x0E);

    // only header type 0x00 ("normal" device) has the 6-BAR layout at
    // 0x10-0x24. bridges (type 0x01) and cardbus (0x02) use those bytes
    // differently, so leave bars[] zeroed for them - nothing in this
    // driver needs to map a bridge's BARs.
    if ((dev->header_type & 0x7F) == 0x00) {
        for (int i = 0; i < 6; i++) {
            dev->bars[i] = pci_config_read32(bus, device, function, 0x10 + i * 4);
        }
    } else {
        for (int i = 0; i < 6; i++) {
            dev->bars[i] = 0;
        }
    }

    device_count++;
    return 1;
}

unsigned int pci_scan(void)
{
    device_count = 0;

    for (unsigned int bus = 0; bus < 256 && device_count < PCI_MAX_DEVICES; bus++) {
        for (unsigned int device = 0; device < 32 && device_count < PCI_MAX_DEVICES; device++) {
            unsigned short vendor_id = pci_config_read16((unsigned char)bus, (unsigned char)device, 0, 0x00);
            if (vendor_id == PCI_VENDOR_NONE) {
                continue; // no device in this slot at all
            }

            unsigned char header_type = pci_config_read8((unsigned char)bus, (unsigned char)device, 0, 0x0E);
            unsigned int function_count = (header_type & 0x80) ? 8 : 1; // bit7 = multifunction

            for (unsigned int function = 0; function < function_count && device_count < PCI_MAX_DEVICES; function++) {
                pci_record_device((unsigned char)bus, (unsigned char)device, (unsigned char)function);
            }
        }
    }

    return device_count;
}

unsigned int pci_get_device_count(void)
{
    return device_count;
}

pci_device_t* pci_get_device(unsigned int index)
{
    if (index >= device_count) {
        return (pci_device_t*)0;
    }
    return &devices[index];
}

pci_device_t* pci_find_device(unsigned char class_code, unsigned char subclass, unsigned char prog_if)
{
    for (unsigned int i = 0; i < device_count; i++) {
        pci_device_t* dev = &devices[i];

        if (class_code != 0xFF && dev->class_code != class_code) continue;
        if (subclass   != 0xFF && dev->subclass   != subclass)   continue;
        if (prog_if    != 0xFF && dev->prog_if    != prog_if)    continue;

        return dev;
    }
    return (pci_device_t*)0;
}

unsigned int pci_get_bar_address(pci_device_t* dev, int bar_index)
{
    if (dev == (pci_device_t*)0 || bar_index < 0 || bar_index > 5) {
        return 0;
    }

    unsigned int bar = dev->bars[bar_index];

    if (bar & 0x1) {
        return 0; // I/O-space BAR, not a memory address
    }

    unsigned int type = (bar >> 1) & 0x3; // 0 = 32-bit, 2 = 64-bit, 1 = reserved
    unsigned int base = bar & ~0xFu;      // low 4 bits are flags, not address

    if (type == 0x2) {
        if (bar_index >= 5) {
            return 0; // malformed - 64-bit BAR with no room for the upper half
        }
        unsigned int upper = dev->bars[bar_index + 1];
        if (upper != 0) {
            return 0; // address is above 4GB - can't map it on a 32-bit kernel
        }
    }

    return base;
}

unsigned int pci_get_bar_size(pci_device_t* dev, int bar_index)
{
    if (dev == (pci_device_t*)0 || bar_index < 0 || bar_index > 5) {
        return 0;
    }

    unsigned int original = dev->bars[bar_index];
    if (original & 0x1) {
        return 0; // I/O-space BAR, not what this is for
    }

    unsigned char offset = (unsigned char)(0x10 + bar_index * 4);

    pci_config_write32(dev->bus, dev->device, dev->function, offset, 0xFFFFFFFFu);
    unsigned int probe = pci_config_read32(dev->bus, dev->device, dev->function, offset);
    pci_config_write32(dev->bus, dev->device, dev->function, offset, original); // restore - leave no trace

    unsigned int size_mask = probe & ~0xFu; // low 4 bits are type/flag bits, not address
    if (size_mask == 0) {
        return 0; // BAR doesn't implement any address bits - shouldn't happen for real hardware
    }

    return (~size_mask) + 1;
}

void pci_enable_device(pci_device_t* dev)
{
    if (dev == (pci_device_t*)0) {
        return;
    }

    unsigned short command = pci_config_read16(dev->bus, dev->device, dev->function, 0x04);
    command |= (1 << 1); // memory space enable
    command |= (1 << 2); // bus master enable
    pci_config_write16(dev->bus, dev->device, dev->function, 0x04, command);
}