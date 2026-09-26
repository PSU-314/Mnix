#include "types.h"
#include "riscv.h"
#include "memlayout.h"
#include "defs.h"
#include "pci.h"

// compute the ECAM address of the start of config space for a given
// (bus, device, function). bus is always 0 in this setup, since qemu's
// virt machine gives us a single-segment, single-bus GPEX root complex
// with no secondary bridges.
static volatile uint32 *pci_cfg_addr(int bus, int dev, int func) {
    uint64 offset =
        ((uint64)bus << 20) | ((uint64)dev << 15) | ((uint64)func << 12);
    return (volatile uint32 *)(ECAM_BASE + offset);
}

// read a 32-bit register from a function's config space, at the given
// byte offset (must be a multiple of 4).
static uint32 pci_cfg_read32(int bus, int dev, int func, int offset) {
    volatile uint32 *base = pci_cfg_addr(bus, dev, func);
    return base[offset / 4];
}

// write a 32-bit register into a function's config space.
static __attribute__((unused)) void pci_cfg_write32(int bus, int dev, int func,
                                                    int offset, uint32 value) {
    volatile uint32 *base = pci_cfg_addr(bus, dev, func);
    base[offset / 4] = value;
}

// scan bus 0, device 0..31, function 0, looking for a device.
// on qemu's virt machine with a single "-device e1000", this will
// find exactly one populated slot.
void pci_init(void) {
    printk("pci: scanning bus 0\n");

    for (int dev = 0; dev < 32; dev++) {
        uint32 id = pci_cfg_read32(0, dev, 0, PCI_CFG_VENDOR_ID);
        uint16 vendor_id = id & 0xFFFF;
        uint16 device_id = (id >> 16) & 0xFFFF;

        if (vendor_id == 0xFFFF) {
            // no device in this slot -- ECAM reads back all-ones.
            continue;
        }

        printk("pci: found device at slot %d: vendor=0x%x device=0x%x\n", dev,
               vendor_id, device_id);

        if (vendor_id == PCI_VENDOR_INTEL && device_id == PCI_DEVICE_E1000) {
            printk("pci: this is the e1000\n");
            // BAR programming and command-register enable will go here
            // in the next step -- for now, discovery stops at identification.
        }
    }
}
