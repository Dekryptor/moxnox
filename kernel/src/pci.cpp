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
import sata;
import event;
import nvme;

using namespace std;

static list_entry pci_device_list = { &pci_device_list, &pci_device_list };
atomic<unsigned int> devices_left = 0;
static object_ref init_device_event;

static constexpr uint64_t pci_pa(const ACPI_MCFG_ALLOCATION& item, uint8_t bus, uint8_t device, uint8_t function) {
    return item.Address | (uint64_t)((bus - item.StartBusNumber) << 20) | (uint64_t)(device << 15) | (uint64_t)(function << 12);
}

static void device_found(const volatile pci_header& h, uint8_t bus, uint8_t dev, uint8_t func) {
    print("{:02x}:{:02x}:{:x} {:04x}:{:04x}\n", bus, dev, func, h.vendor_id, h.device_id);

    // FIXME - header type 1 (PCI-to-PCI bridge)
    // FIXME - header type 2 (PCI-to-CardBus bridge)

    if ((h.header_type & 0x7f) != 0) {
        print("Unhandled PCI header type {:x}\n", (uint8_t)(h.header_type & 0x7f));
        return;
    }

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

            // FIXME - MSI-X if supported as well

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

static void check_bus(uint8_t bus, const ACPI_MCFG_ALLOCATION& item) {
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

        device_found(h, bus, dev, 0);

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

                if (h.vendor_id == 0xffff) {
                    unmap_page(va);
                    free_va(va, 1);
                    continue;
                }

                device_found(h, bus, dev, func);
            }
        }
    }
}

static void check_buses(const ACPI_MCFG_ALLOCATION& item) {
    // FIXME - multiple PCI controllers
    // FIXME - PCI-to-PCI bridges

    check_bus(item.StartBusNumber, item);
}

static void pci_device_found(void* ctx) {
    auto& dev = *(pci_device*)ctx;

    switch (dev.header->class_code) {
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
