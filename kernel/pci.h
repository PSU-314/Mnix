#ifndef _PCI_H_
#define _PCI_H_

#include "types.h"

// Intel's PCI vendor ID -- fixed by the PCI-SIG vendor ID registry.
#define PCI_VENDOR_INTEL 0x8086

// device ID for the 82540EM, which is what qemu's "-device e1000" emulates.
#define PCI_DEVICE_E1000 0x100E

// standard PCI config space header offsets (byte offsets into
// each function's config space region).
#define PCI_CFG_VENDOR_ID 0x00 // 16-bit vendor ID (low half of first dword)
#define PCI_CFG_DEVICE_ID 0x02 // 16-bit device ID (high half of first dword)
#define PCI_CFG_COMMAND 0x04   // 16-bit command register
#define PCI_CFG_STATUS 0x06    // 16-bit status register
#define PCI_CFG_BAR0 0x10      // first Base Address Register

// PCI command register bits (offset 0x04) -- these enable the
// device to actually respond to memory and bus-master transactions.
#define PCI_CMD_MEM_ENABLE (1 << 1) // respond to memory space accesses
#define PCI_CMD_BUS_MASTER (1 << 2) // allow the device to initiate DMA

void pci_init(void);

#endif
