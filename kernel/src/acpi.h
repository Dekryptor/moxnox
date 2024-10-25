#pragma once

#pragma pack(push,1)

typedef struct {
    uint32_t Signature;
    uint32_t Length;
    uint8_t Revision;
    uint8_t Checksum;
    char OEMID[6];
    char OEMTableID[8];
    uint32_t OEMRevision;
    uint32_t CreatorID;
    uint32_t CreatorRevision;
} ACPISDTHeader;

enum class madt_type : uint8_t {
    local_apic = 0,
    io_apic = 1,
    interrupt_source_override = 2,
    nmi_source = 3,
    local_apic_nmi = 4,
    local_apic_address_override = 5,
    io_sapic = 6,
    local_sapic = 7,
    platform_interrupt_sources = 8,
    local_x2apic = 9,
    local_x2apic_nmi = 10,
    gicc = 11,
    gicd = 12,
    gic_msi_frame = 13,
    gicr = 14,
    gic_its = 15
};

typedef struct {
    uint8_t id;
    uint8_t reserved;
    uint32_t address;
    uint32_t interrupt_base;
} madt_ioapic;

typedef struct {
    uint8_t bus_source;
    uint8_t irq_source;
    uint32_t gsi;
    uint16_t flags;
} madt_source_override;

typedef struct {
    uint8_t processor_id;
    uint8_t apic_id;
    uint32_t enabled : 1;
    uint32_t online_capable : 1;
    uint32_t reserved : 30;
} madt_local_apic;

struct apic_mcfg_item {
    uint64_t base_address;
    uint16_t segment_group_number;
    uint8_t start_number;
    uint8_t end_number;
    uint32_t reserved;
};

struct apic_mcfg {
    ACPISDTHeader header;
    uint64_t reserved;
    apic_mcfg_item items[1];
};

enum class acpi_address_space : uint8_t {
    system_memory = 0x00,
    system_io = 0x01,
    pci_config = 0x02,
    embedded_controller = 0x03,
    smbus = 0x04,
    system_cmos = 0x05,
    pci_bar_target = 0x06,
    ipmi = 0x07,
    gpio = 0x08,
    generic_serial_bus = 0x09,
    pcc = 0xa,
    functional_fixed_hw = 0x7f,
};

struct GenericAddressStructure {
    enum acpi_address_space AddressSpace;
    uint8_t BitWidth;
    uint8_t BitOffset;
    uint8_t AccessSize;
    uint64_t Address;
};

struct fadt {
    ACPISDTHeader h;
    uint32_t FirmwareCtrl;
    uint32_t Dsdt;
    uint8_t Reserved;
    uint8_t PreferredPowerManagementProfile;
    uint16_t SCI_Interrupt;
    uint32_t SMI_CommandPort;
    uint8_t AcpiEnable;
    uint8_t AcpiDisable;
    uint8_t S4BIOS_REQ;
    uint8_t PSTATE_Control;
    uint32_t PM1aEventBlock;
    uint32_t PM1bEventBlock;
    uint32_t PM1aControlBlock;
    uint32_t PM1bControlBlock;
    uint32_t PM2ControlBlock;
    uint32_t PMTimerBlock;
    uint32_t GPE0Block;
    uint32_t GPE1Block;
    uint8_t PM1EventLength;
    uint8_t PM1ControlLength;
    uint8_t PM2ControlLength;
    uint8_t PMTimerLength;
    uint8_t GPE0Length;
    uint8_t GPE1Length;
    uint8_t GPE1Base;
    uint8_t CStateControl;
    uint16_t WorstC2Latency;
    uint16_t WorstC3Latency;
    uint16_t FlushSize;
    uint16_t FlushStride;
    uint8_t DutyOffset;
    uint8_t DutyWidth;
    uint8_t DayAlarm;
    uint8_t MonthAlarm;
    uint8_t Century;
    uint16_t BootArchitectureFlags;
    uint8_t Reserved2;
    uint32_t Flags;
    GenericAddressStructure ResetReg;
    uint8_t ResetValue;
    uint8_t Reserved3[3];
    uint64_t X_FirmwareControl;
    uint64_t X_Dsdt;
    GenericAddressStructure X_PM1aEventBlock;
    GenericAddressStructure X_PM1bEventBlock;
    GenericAddressStructure X_PM1aControlBlock;
    GenericAddressStructure X_PM1bControlBlock;
    GenericAddressStructure X_PM2ControlBlock;
    GenericAddressStructure X_PMTimerBlock;
    GenericAddressStructure X_GPE0Block;
    GenericAddressStructure X_GPE1Block;
};

#pragma pack(pop)

extern void* acpi_tables;
extern unsigned int acpi_count;

static const uint32_t ACPI_APIC = 0x43495041;
static const uint32_t ACPI_MCFG = 0x4746434d;
static const uint32_t ACPI_FACP = 0x50434146;

static void find_acpi_table(uint32_t sig, std::invocable<const ACPISDTHeader&> auto func) {
    auto ptr = (uint8_t*)acpi_tables;

    for (unsigned int i = 0; i < acpi_count; i++) {
        auto& tbl = *(ACPISDTHeader*)ptr;

        // FIXME - check checksums

        if (tbl.Signature == sig)
            func(tbl);

        ptr += tbl.Length;
    }
}
