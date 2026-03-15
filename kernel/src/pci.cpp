module;

#include <span>
#include <optional>
#include <array>
#include <stdint.h>
#include "pci.h"

export module pci;

import list;
import interrupts;

using namespace std;

export bool init_pci_done = false;

export struct msix_entry {
    uint32_t message_address_low;
    uint32_t message_address_high;
    uint32_t message_data;
    uint32_t vector_control;
} __attribute__((packed));

static_assert(sizeof(msix_entry) == 16);

export struct pci_capability {
    enum pci_capability_type type;
    uint8_t next;
} __attribute__((packed));

static_assert(sizeof(pci_capability) == 2);

export struct pci_msi : pci_capability {
    uint16_t message_control;
    uint32_t message_address_low;
    union {
        struct {
            uint16_t message_data;
        } v32;
        struct {
            uint32_t message_address_high;
            uint16_t message_data;
            uint16_t reserved;
            uint32_t mask;
            uint32_t pending;
        } v64;
    };
} __attribute__((packed));

static_assert(sizeof(pci_msi) == 24);

struct pci_msix : pci_capability {
    uint16_t message_control;
    uint32_t bir : 3;
    uint32_t table_offset : 29;
    uint32_t pending_bit_bir : 3;
    uint32_t pending_bit_offset : 29;
} __attribute__((packed));

static_assert(sizeof(pci_msix) == 12);

export struct vendor_specific_cap {
    list_entry le;
    void* ptr;
};

export enum class pci_class : uint8_t {
    unclassified = 0,
    mass_storage = 1,
    network = 2,
    display = 3,
    multimedia = 4,
    memory = 5,
    bridge = 6,
    simple_communication = 7,
    base_system_peripheral = 8,
    input_device = 9,
    docking_station = 10,
    processor = 11,
    serial_bus = 12,
    wireless = 13,
    intelligent = 14,
    satellite_communication = 15,
    encryption = 16,
    signal_processing = 17
};

export enum class pci_mass_storage : uint8_t {
    scsi_bus = 0,
    ide = 1,
    floppy_disk = 2,
    ipi_bus = 3,
    raid = 4,
    ata = 5,
    sata = 6,
    serial_attached_scsi = 7,
    nonvolatile_memory = 8
};

export enum class pci_serial_bus : uint8_t {
    firewire = 0,
    access = 1,
    ssa = 2,
    usb = 3,
    fibre_channel = 4,
    smbus = 5,
    infiniband = 6,
    ipmi = 7,
    sercos = 8,
    canbus = 9
};

export enum class pci_base_system_peripheral : uint8_t {
    pic = 0,
    dma = 1,
    timer = 2,
    rtc = 3,
    pci_hotplug = 4,
    sdhci = 5
};

struct pci_header {
    uint16_t vendor_id;
    uint16_t device_id;
    uint16_t command;
    uint16_t status;
    uint8_t revision_id;
    uint8_t prog_if;
    uint8_t sub_class;
    enum pci_class class_code;
    uint8_t cache_line_size;
    uint8_t latency_timer;
    uint8_t header_type;
    uint8_t bist;
} __attribute__((packed));

static_assert(sizeof(pci_header) == 0x10);

struct pci_header0 : pci_header {
    uint32_t bar[6];
    uint32_t cardbus_cis_pointer;
    uint16_t subsystem_vendor_id;
    uint16_t subsystem_id;
    uint32_t expansion_rom_base_address;
    uint8_t capabilities_pointer;
    uint8_t reserved1[3];
    uint32_t reserved2;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint8_t min_grant;
    uint8_t max_latency;
};

static_assert(sizeof(pci_header0) == 0x40);

struct pci_header1 : pci_header {
    uint32_t bar[2];
    uint8_t primary_bus_number;
    uint8_t secondary_bus_number;
    uint8_t subordinate_bus_number;
    uint8_t secondary_latency_timer;
    uint8_t io_base;
    uint8_t io_limit;
    uint16_t secondary_status;
    uint16_t memory_base;
    uint16_t memory_limit;
    uint16_t prefetchable_memory_base;
    uint16_t prefetchable_memory_limit;
    uint32_t prefetchable_base_upper;
    uint32_t prefetchable_limit_upper;
    uint16_t io_base_upper;
    uint16_t io_limit_upper;
    uint8_t capability_pointer;
    uint8_t reserved[3];
    uint32_t expansion_rom;
    uint8_t interrupt_line;
    uint8_t interrupt_pin;
    uint16_t bridge_control;
};

static_assert(sizeof(pci_header1) == 0x40);

export struct pci_device {
    ~pci_device();

    list_entry le;
    volatile pci_header0* header;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    uint8_t pin;
    optional<uint32_t> irq;
    array<span<volatile uint8_t>, 6> bar;
    span<volatile uint8_t> msi;
    volatile pci_msix* msix = nullptr;
    unsigned int num;
    bool initialized = false;
    list_entry vendor_specific_caps = { &vendor_specific_caps, &vendor_specific_caps };
};

const uint16_t PCI_COMMAND_MEMORY = 0x0002;
const uint16_t PCI_COMMAND_MASTER = 0x0004;
export const uint16_t PCI_COMMAND_INTX_DISABLE = 0x0400;

export struct pci_recognizer {
    list_entry le;
    uint16_t vendor_id;
    uint16_t device_id;
    bool early;
    void (*func)(pci_device&);
};

export list_entry pci_ids = { &pci_ids, &pci_ids };

export struct pci_class_recognizer {
    list_entry le;
    enum pci_class pci_class;
    uint8_t pci_subclass;
    bool early;
    bool (*func)(const pci_device&);
};

export list_entry pci_classes = { &pci_classes, &pci_classes };

export constexpr uint32_t apic_msi_address(uint8_t processor, bool rh, bool dm) {
    return 0xfee00000 | (uint32_t)(processor << 12) | (rh ? 0x8 : 0x0) | (dm ? 0x4 : 0x0);
}

export void init_pci(bool early);
export uint16_t setup_msi(const pci_device& dev, bool msix, irq_func_type func,
                          void* ctx);
export uint16_t add_msix_entry(const pci_device& dev, uint16_t vec,
                               irq_func_type func, void* ctx);
