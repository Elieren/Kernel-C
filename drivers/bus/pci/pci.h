#ifndef PCI_H
#define PCI_H

#include <stdint.h>

#define PCI_MAX_DEVICES 256

typedef struct
{
    uint8_t bus;
    uint8_t device;
    uint8_t function;

    uint16_t vendor_id;
    uint16_t device_id;
    uint8_t class_code;
    uint8_t subclass;
    uint8_t prog_if;
    uint8_t header_type;

    uint64_t bar_addr[6];
    uint64_t bar_size[6];
    uint8_t bar_is_io[6];
} pci_device_t;

void pci_init(void);
int pci_get_device_count(void);
pci_device_t *pci_get_device(int idx);

#endif /* PCI_H */