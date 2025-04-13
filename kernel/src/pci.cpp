module;

#include "kernel.h"
#include "acpica.h"
#include "pci.h"
import objects;
#include "handle.h"

export module pci;

import print;
import mem;
import config;
import list;
import event;

using namespace std;

static list_entry pci_device_list = { &pci_device_list, &pci_device_list };
atomic<unsigned int> devices_left = 0;
static object_ref init_device_event;

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

export struct pci_device {
    ~pci_device();

    list_entry le;
    volatile pci_header0* header;
    uint8_t bus;
    uint8_t device;
    uint8_t function;
    array<span<volatile uint8_t>, 6> bar;
    span<volatile uint8_t> msi;
    volatile pci_msix* msix = nullptr;
    unsigned int num;
    list_entry vendor_specific_caps = { &vendor_specific_caps, &vendor_specific_caps };
};

export const uint16_t PCI_COMMAND_MASTER = 0x0004;
export const uint16_t PCI_COMMAND_INTX_DISABLE = 0x0400;

export struct pci_recognizer {
    list_entry le;
    uint16_t vendor_id;
    uint16_t device_id;
    void (*func)(pci_device&);
};

export list_entry pci_ids = { &pci_ids, &pci_ids };

struct check_bus_entry {
    list_entry le;
    uint8_t num;
};

// nvme.cpp
extern "C++" void init_nvme(const pci_device& dev);

// sata.cpp
extern "C++" void init_sata(const pci_device& dev);

// xhci.cpp
extern "C++" void init_xhci(const pci_device& dev);

export constexpr uint32_t apic_msi_address(uint8_t processor, bool rh, bool dm) {
    return 0xfee00000 | (uint32_t)(processor << 12) | (rh ? 0x8 : 0x0) | (dm ? 0x4 : 0x0);
}

static constexpr uint64_t pci_pa(const ACPI_MCFG_ALLOCATION& item, uint8_t bus, uint8_t device, uint8_t function) {
    return item.Address | (uint64_t)((bus - item.StartBusNumber) << 20) | (uint64_t)(device << 15) | (uint64_t)(function << 12);
}

static void device_found0(const volatile pci_header& h, uint8_t bus, uint8_t dev, uint8_t func) {
    volatile auto& h0 = *(pci_header0*)&h;

    auto ppci_dev = new pci_device;
    if (!ppci_dev) {
        print("new failed\n");
        return;
    }

    auto& pci_dev = *ppci_dev;
    pci_dev.header = &h0;
    pci_dev.bus = bus;
    pci_dev.device = dev;
    pci_dev.function = func;

    if (h0.status & 0x10 && (h0.capabilities_pointer & ~0x3) != 0) {
        auto ptr = (uint8_t*)((uint8_t*)&h0 + (h0.capabilities_pointer & ~0x3));

        while (true) {
            auto& cap = *(pci_capability*)ptr;

            switch (cap.type) {
                case pci_capability_type::msi: {
                    volatile auto& msi = *(pci_msi*)ptr;
                    size_t len;

                    if (msi.message_control & 0x80) { // 64-bit
                        if (msi.message_control & 0x100) // per-vector masking
                            len = sizeof(pci_msi);
                        else
                            len = offsetof(pci_msi, v64.reserved);
                    } else
                        len = offsetof(pci_msi, v32.message_data) + sizeof(uint16_t);

                    pci_dev.msi = span((volatile uint8_t*)((uintptr_t)&h | ((uintptr_t)ptr & (PAGE_SIZE - 1))), len);
                    break;
                }

                case pci_capability_type::msix:
                    pci_dev.msix = (volatile pci_msix*)((uintptr_t)&h | ((uintptr_t)ptr & (PAGE_SIZE - 1)));
                    break;

                case pci_capability_type::vendor_specific: {
                    auto ent = new vendor_specific_cap;
                    if (!ent) {
                        print("new failed\n");
                        return;
                    }

                    ent->ptr = (void*)((uintptr_t)&h | ((uintptr_t)(ptr + sizeof(pci_capability)) & (PAGE_SIZE - 1)));

                    insert_tail_list(pci_dev.vendor_specific_caps, ent->le);
                }

                default:
                    break;
            }

            if (cap.next == 0)
                break;

            ptr = (uint8_t*)((uint8_t*)&h0 + (cap.next & ~0x3));
        }
    }

    for (unsigned int i = 0; i < 6; i++) {
        volatile auto& b = h0.bar[i];

        if (b != 0) {
            uint32_t orig = b;

            if (!(b & 1)) { // memory space
                auto type = (orig >> 1) & 0x3;

                if (type != 0 && type != 2) {
                    print("Unhandled PCI BAR type {:x}\n", type);
                    continue;
                }

                b = 0xffffffff;
                uint32_t changed = b;
                b = orig;

                uint64_t end = orig + ~changed + 1;

                if (type == 0) {
                    auto pa = orig & 0xfffffff0;
                    auto pages = bytes_to_pages(end - pa);
                    auto va = allocate_va(pages);

                    if (!va) {
                        print("allocate_va failed\n");
                        return;
                    }

                    for (unsigned int j = 0; j < pages; j++) {
                        // FIXME - if prefetchable, map as write-through (or write-combine if framebuffer)?
                        if (!map_page(pa + (j << PAGE_SHIFT), va + (j << PAGE_SHIFT), mapping_type::no_cache)) {
                            print("map_page failed\n");
                            return;
                        }
                    }

                    pci_dev.bar[i] = span((volatile uint8_t*)va, end - pa);
                } else { // 64-bit register
                    if (i == 5) {
                        print("64-bit PCI register cannot occupy BAR slot 5.\n");
                        continue;
                    }

                    // FIXME - what if BAR crosses 32-bit boundary?

                    uint64_t pa = orig & 0xfffffff0;

                    pa |= (uint64_t)h0.bar[i+1] << 32;
                    end |= (uint64_t)h0.bar[i+1] << 32;

                    auto pages = bytes_to_pages(end - pa);
                    auto va = allocate_va(pages);

                    if (!va) {
                        print("allocate_va failed\n");
                        return;
                    }

                    for (unsigned int j = 0; j < pages; j++) {
                        // FIXME - if prefetchable, map as write-through (or write-combine if framebuffer)?
                        if (!map_page(pa + (j << PAGE_SHIFT), va + (j << PAGE_SHIFT), mapping_type::no_cache)) {
                            print("map_page failed\n");
                            return;
                        }
                    }

                    pci_dev.bar[i] = span((volatile uint8_t*)va, end - pa);

                    i++;
                }
            }
        }
    }

    insert_tail_list(pci_device_list, pci_dev.le);
}

static void device_found1(const volatile pci_header& h, list_entry& buses) {
    volatile auto& h1 = *(pci_header1*)&h;

    if (h1.class_code != pci_class::bridge)
        return;

    if ((enum pci_bridge)h1.sub_class != pci_bridge::pci_to_pci_bridge)
        return;

    auto e = new check_bus_entry;
    if (!e) {
        print("malloc failed\n");
        return;
    }

    e->num = h1.secondary_bus_number;
    insert_tail_list(buses, e->le);
}

static void device_found(const volatile pci_header& h, uint8_t bus, uint8_t dev,
                         uint8_t func, list_entry& buses) {
    print("{:02x}:{:02x}:{:x} {:04x}:{:04x}\n", bus, dev, func, h.vendor_id, h.device_id);

    switch (h.header_type & 0x7f) {
        case 0:
            device_found0(h, bus, dev, func);
            break;

        case 1:
            device_found1(h, buses);
            break;

        // FIXME - header type 2? (PCI-to-CardBus bridge)

        default:
            print("Unhandled PCI header type {:x}\n", (uint8_t)(h.header_type & 0x7f));
            break;
    }
}

static void check_bus(uint8_t bus, const ACPI_MCFG_ALLOCATION& item,
                      list_entry& buses) {
    // FIXME - handle VA leaks?

    for (uint8_t dev = 0; dev < 32; dev++) {
        auto pa = pci_pa(item, bus, dev, 0);

        auto va = allocate_va(1);
        if (!va) {
            print("allocate_va failed\n");
            return;
        }

        if (!map_page(pa, va, mapping_type::no_cache)) {
            print("map_page failed\n");
            free_va(va, 1);
            return;
        }

        const volatile auto& h = *(pci_header*)va;

        if (h.vendor_id == 0xffff) {
            unmap_page(va);
            free_va(va, 1);
            continue;
        }

        bool multifunction = h.header_type & 0x80;

        device_found(h, bus, dev, 0, buses);

        if (multifunction) {
            for (uint8_t func = 1; func < 8; func++) {
                pa = pci_pa(item, bus, dev, func);

                va = allocate_va(1);
                if (!va) {
                    print("allocate_va failed\n");
                    return;
                }

                if (!map_page(pa, va, mapping_type::no_cache)) {
                    print("map_page failed\n");
                    free_va(va, 1);
                    return;
                }

                const volatile auto& h2 = *(pci_header*)va;

                if (h2.vendor_id == 0xffff) {
                    unmap_page(va);
                    free_va(va, 1);
                    continue;
                }

                device_found(h2, bus, dev, func, buses);
            }
        }
    }
}

static void check_buses(const ACPI_MCFG_ALLOCATION& item) {
    list_entry buses = { &buses, &buses };

    {
        auto e = new check_bus_entry;
        if (!e) {
            print("malloc failed\n");
            return;
        }

        e->num = item.StartBusNumber;
        insert_tail_list(buses, e->le);
    }

    while (!is_list_empty(buses)) {
        auto& e = containing_record<check_bus_entry>(buses.next);

        remove_entry_list(e.le);

        check_bus(e.num, item, buses);

        free(&e);
    }
}

static void pci_device_found(void* ctx) {
    auto& dev = *(pci_device*)ctx;

    bool found = false;
    auto vendor_id = dev.header->vendor_id;
    auto device_id = dev.header->device_id;

    for (auto le = pci_ids.next; le != &pci_ids; le = le->next) {
        auto& ent = containing_record<pci_recognizer>(le);

        if (ent.vendor_id == vendor_id && ent.device_id == device_id) {
            ent.func(dev);
            found = true;
            break;
        }
    }

    if (!found) {
        switch (dev.header->class_code) {
            case pci_class::serial_bus:
                switch ((enum pci_serial_bus)dev.header->sub_class) {
                    case pci_serial_bus::usb:
                        switch (dev.header->prog_if) {
                            case 0x30:
                                init_xhci(dev);
                            break;
                        }
                        break;

                    default:
                        break;
                }

                break;
            break;

            case pci_class::mass_storage: {
                switch ((enum pci_mass_storage)dev.header->sub_class) {
                    case pci_mass_storage::sata:
                        init_sata(dev);
                    break;

                    case pci_mass_storage::nonvolatile_memory:
                        init_nvme(dev);
                    break;

                    default:
                        break;
                }

                break;
            }

            default:
                break;
        }
    }

    if (--devices_left == 0)
        set_event(static_cast<event&>(init_device_event.get()));
}

export void init_pci() {
    ACPI_TABLE_HEADER* tbl;
    auto status = AcpiGetTable((char*)ACPI_SIG_MCFG, 1, &tbl);

    if (status == AE_NOT_FOUND) // no PCIe
        return;
    else if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MCFG\n", status);
        return;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_MCFG)) {
        print("MCFG table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_MCFG));
        return;
    }

    auto items = span((ACPI_MCFG_ALLOCATION*)((uint8_t*)tbl + sizeof(ACPI_TABLE_MCFG)),
                      (tbl->Length - sizeof(ACPI_TABLE_MCFG)) / sizeof(ACPI_MCFG_ALLOCATION));

    if (items.empty()) {
        print("No ACPI_MCFG_ALLOCATIONs found\n");
        return;
    }

    for (const auto& item: items) {
        check_buses(item);
    }

    {
        auto le = pci_device_list.next;
        while (le != &pci_device_list) {
            devices_left++;
            le = le->next;
        }
    }

    if (devices_left == 0)
        return;

    init_device_event = create_event(true);

    unsigned int nvme_num = 0;

    for (auto le = pci_device_list.next; le != &pci_device_list; le = le->next) {
        auto& dev = containing_record<pci_device>(le);

        if (dev.header->class_code == pci_class::mass_storage &&
          (enum pci_mass_storage)dev.header->sub_class == pci_mass_storage::nonvolatile_memory) {
            dev.num = nvme_num;
            nvme_num++;
        }
    }

    for (auto le = pci_device_list.next; le != &pci_device_list; le = le->next) {
        auto& dev = containing_record<pci_device>(le);

        add_kernel_thread(pci_device_found, &dev);
    }

    wait_for_object(init_device_event.get(), false);
    init_device_event.reset();
}

pci_device::~pci_device() {
    while (!is_list_empty(vendor_specific_caps)) {
        auto& ent = containing_record<vendor_specific_cap>(vendor_specific_caps.next);

        remove_entry_list(ent.le);

        delete &ent;
    }
}
