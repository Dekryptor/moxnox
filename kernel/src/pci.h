#pragma once

enum class pci_bridge {
    host_bridge = 0x00,
    isa_bridge = 0x01,
    eisa_bridge = 0x02,
    mca_bridge = 0x03,
    pci_to_pci_bridge = 0x04,
    pcmcia_bridge = 0x05,
    nubus_bridge = 0x06,
    cardbus_bridge = 0x07,
    raceway_bridge = 0x08,
    semitransparent_pci_to_pci = 0x09,
    infiniband_to_pci = 0x0a,
    other = 0x80
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
