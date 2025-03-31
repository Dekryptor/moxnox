#pragma once

enum class pci_class : uint8_t {
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

struct pci_msix;

struct pci_device {
    list_entry le;
    volatile pci_header0* header;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    std::span<volatile uint8_t> bar[6];
    std::span<volatile uint8_t> msi;
    volatile pci_msix* msix = nullptr;
};

enum class pci_mass_storage {
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

enum class pci_capability_type : uint8_t {
    null = 0,
    power_management = 1,
    agp = 2,
    vpd = 3,
    slot_identification = 4,
    msi = 5,
    compactpci_hot_swap = 6,
    pcix = 7,
    hypertransport = 8,
    vendor_specific = 9,
    debug_port = 10,
    compactpci_central_resource_control = 11,
    pci_hotplug = 12,
    pci_bridge_subsystem_vendor_id = 13,
    agp8x = 14,
    secure_device = 15,
    pci_express = 16,
    msix = 17,
    sata_data_index_configuration = 18,
    advanced_features = 19,
    enhanced_allocation = 20,
    flattening_portal_bridge = 21
};

struct pci_capability {
    enum pci_capability_type type;
    uint8_t next;
} __attribute__((packed));

static_assert(sizeof(pci_capability) == 2);

struct pci_msi : pci_capability {
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
