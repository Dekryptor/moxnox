module;

#include <span>
#include <string_view>
#include <atomic>
#include <mutex>
#include "acpica.h"
#include "tick_rate.h"
#include "spinlock.h"
#include "scheduler.h"

export module acpi;

#ifndef __aarch64__
import inout;
import semaphore;
import rwsem;
import apic;
#endif

import format;
import print;
import mem;
import list;
import thread;
import interrupts;
import timer;

using namespace std;

export uint64_t rsdp_pa;

struct acpi_irq_handler {
    list_entry le;
    ACPI_OSD_HANDLER service_routine;
    void* context;
};

struct acpi_irq_num {
    list_entry le;
    uint32_t irq;
    list_entry handlers = { &handlers, &handlers };
};

#ifndef __aarch64__
static rwsem irq_handlers_lock;
static list_entry irq_handlers = { &irq_handlers, &irq_handlers };
#endif

export struct acpi_device {
    list_entry le;
    string_view acpi_name;
    string_view hid;
    string_view uid;
    list_entry resources = { &resources, &resources };
    span<string_view> cids;
    char buf[0];
};

export enum class acpi_device_resource_type {
    io,
    irq,
    extended_irq,
    address16,
    address32,
    address64,
    fixed_memory32,
    dma,
    gpio,
    i2c_serial_bus,
    spi_serial_bus,
    uart_serial_bus,
    csi2_serial_bus,
};

export struct acpi_device_resource {
    list_entry le;
    enum acpi_device_resource_type type;
};

export struct acpi_device_resource_io : acpi_device_resource {
    ACPI_RESOURCE_IO io;
};

export struct acpi_device_resource_irq : acpi_device_resource {
    ACPI_RESOURCE_IRQ irq;
};

export struct acpi_device_resource_extended_irq : acpi_device_resource {
    ACPI_RESOURCE_EXTENDED_IRQ extended_irq;
};

export struct acpi_device_resource_address16 : acpi_device_resource {
    ACPI_RESOURCE_ADDRESS16 address16;
};

export struct acpi_device_resource_address32 : acpi_device_resource {
    ACPI_RESOURCE_ADDRESS32 address32;
};

export struct acpi_device_resource_address64 : acpi_device_resource {
    ACPI_RESOURCE_ADDRESS64 address64;
};

export struct acpi_device_resource_fixed_memory32 : acpi_device_resource {
    ACPI_RESOURCE_FIXED_MEMORY32 fixed_memory32;
};

export struct acpi_device_resource_dma : acpi_device_resource {
    ACPI_RESOURCE_DMA dma;
};

export struct acpi_device_resource_gpio : acpi_device_resource {
    ACPI_RESOURCE_GPIO gpio;
};

export struct acpi_device_resource_i2c_serial_bus : acpi_device_resource {
    ACPI_RESOURCE_I2C_SERIALBUS i2c_serial_bus;
};

export struct acpi_device_resource_spi_serial_bus : acpi_device_resource {
    ACPI_RESOURCE_SPI_SERIALBUS spi_serial_bus;
};

export struct acpi_device_resource_uart_serial_bus : acpi_device_resource {
    ACPI_RESOURCE_UART_SERIALBUS uart_serial_bus;
};

export struct acpi_device_resource_csi2_serial_bus : acpi_device_resource {
    ACPI_RESOURCE_CSI2_SERIALBUS csi2_serial_bus;
};

static list_entry acpi_devices = { &acpi_devices, &acpi_devices };

export struct acpi_recognizer {
    list_entry le;
    string_view hid;
    void (*func)(const acpi_device&);
    void (*prelim_func)(string_view);
};

export list_entry pnp_hids = { &pnp_hids, &pnp_hids };

export bool acpi_early_init_done = false;
export bool init_acpi_done = false;

static void acpi_notify_handler(ACPI_HANDLE device, uint32_t value, void*) {
    print("FIXME - acpi_notify_handler({}, {})\n", (uintptr_t)device, value);
}

#ifndef __aarch64__
static bool acpi_isr_func(void* ctx) {
    auto& n = *(acpi_irq_num*)ctx;

    lock_guard lock(irq_handlers_lock);

    for (auto le = n.handlers.next; le != &n.handlers; le = le->next) {
        auto& h = containing_record<acpi_irq_handler>(le);

        if (h.service_routine(h.context) == ACPI_INTERRUPT_HANDLED)
            return true;
    }

    return false;
}
#endif

export void init_acpi_early() {
    ACPI_STATUS Status;

    Status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeSubsystem returned {}\n", Status);
        return;
    }

    Status = AcpiInitializeTables(nullptr, 16, false);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeTables returned {}\n", Status);
        return;
    }

    Status = AcpiLoadTables();
    if (ACPI_FAILURE(Status)) {
        print("AcpiLoadTables returned {}\n", Status);
        return;
    }
}

static void power_button_pressed_thread() {
#ifndef __aarch64__ // FIXME
    // FIXME - should be calling NtShutdownSystem (why does it not work?)

    AcpiEnterSleepStatePrep(5);
    __asm("cli");
    AcpiEnterSleepState(5);
#endif

    print("power off\n");
}

static uint32_t power_button_pressed(void*) {
    // interrupts are disabled here
    add_kernel_thread(power_button_pressed_thread);

    return 0;
}

export void init_acpi() {
    ACPI_STATUS Status;

    Status = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_SYSTEM_NOTIFY,
                                      acpi_notify_handler, nullptr);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInstallNotifyHandler returned {}\n", Status);
        return;
    }

    Status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE (Status)) {
        print("AcpiEnableSubsystem returned {}\n", Status);
        return;
    }

    Status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeObjects returned {}\n", Status);
        return;
    }

    Status = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
                                          power_button_pressed, nullptr);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInstallFixedEventHandler returned {}\n", Status);
        return;
    }
}

static ACPI_STATUS find_device_resources_cb(ACPI_RESOURCE* res, void* ctx) {
    auto& d = *(acpi_device*)ctx;

    switch (res->Type) {
        case ACPI_RESOURCE_TYPE_IRQ: {
            auto r = new acpi_device_resource_irq;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::irq;
            memcpy(&r->irq, &res->Data.Irq, sizeof(ACPI_RESOURCE_IRQ));
            insert_tail_list(d.resources, r->le);
            break;
        }

        case ACPI_RESOURCE_TYPE_DMA: {
            auto r = new acpi_device_resource_dma;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::dma;
            memcpy(&r->dma, &res->Data.Dma, sizeof(ACPI_RESOURCE_DMA));
            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_START_DEPENDENT
        // FIXME - ACPI_RESOURCE_TYPE_END_DEPENDENT

        case ACPI_RESOURCE_TYPE_IO: {
            auto r = new acpi_device_resource_io;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::io;
            memcpy(&r->io, &res->Data.Io, sizeof(ACPI_RESOURCE_IO));
            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_FIXED_IO
        // FIXME - ACPI_RESOURCE_TYPE_VENDOR

        case ACPI_RESOURCE_TYPE_END_TAG:
            break;

        // FIXME - ACPI_RESOURCE_TYPE_MEMORY24
        // FIXME - ACPI_RESOURCE_TYPE_MEMORY32

        case ACPI_RESOURCE_TYPE_FIXED_MEMORY32: {
            auto r = new acpi_device_resource_fixed_memory32;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::fixed_memory32;
            memcpy(&r->fixed_memory32, &res->Data.FixedMemory32, sizeof(ACPI_RESOURCE_FIXED_MEMORY32));
            insert_tail_list(d.resources, r->le);
            break;
        }

        case ACPI_RESOURCE_TYPE_ADDRESS16: {
            auto r = new acpi_device_resource_address16;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::address16;
            memcpy(&r->address16, &res->Data.Address16, sizeof(ACPI_RESOURCE_ADDRESS16));
            insert_tail_list(d.resources, r->le);
            break;
        }

        case ACPI_RESOURCE_TYPE_ADDRESS32: {
            auto r = new acpi_device_resource_address32;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::address32;
            memcpy(&r->address32, &res->Data.Address32, sizeof(ACPI_RESOURCE_ADDRESS32));
            insert_tail_list(d.resources, r->le);
            break;
        }

        case ACPI_RESOURCE_TYPE_ADDRESS64: {
            auto r = new acpi_device_resource_address64;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::address64;
            memcpy(&r->address64, &res->Data.Address64, sizeof(ACPI_RESOURCE_ADDRESS64));
            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_EXTENDED_ADDRESS64

        case ACPI_RESOURCE_TYPE_EXTENDED_IRQ: {
            auto r = new acpi_device_resource_extended_irq;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::extended_irq;
            memcpy(&r->extended_irq, &res->Data.ExtendedIrq, sizeof(ACPI_RESOURCE_EXTENDED_IRQ));
            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_GENERIC_REGISTER

        case ACPI_RESOURCE_TYPE_GPIO: {
            auto r = new acpi_device_resource_gpio;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::gpio;
            memcpy(&r->gpio, &res->Data.Gpio, sizeof(ACPI_RESOURCE_GPIO));
            insert_tail_list(d.resources, r->le);
            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_FIXED_DMA

        case ACPI_RESOURCE_TYPE_SERIAL_BUS: {
            switch (res->Data.CommonSerialBus.Type) {
                case ACPI_RESOURCE_SERIAL_TYPE_I2C: {
                    auto r = new acpi_device_resource_i2c_serial_bus;
                    if (!r) {
                        print("out of memory\n");
                        return AE_OK;
                    }

                    r->type = acpi_device_resource_type::i2c_serial_bus;
                    memcpy(&r->i2c_serial_bus, &res->Data.I2cSerialBus,
                        sizeof(ACPI_RESOURCE_I2C_SERIALBUS));
                    insert_tail_list(d.resources, r->le);
                    break;
                }

                case ACPI_RESOURCE_SERIAL_TYPE_SPI: {
                    auto r = new acpi_device_resource_spi_serial_bus;
                    if (!r) {
                        print("out of memory\n");
                        return AE_OK;
                    }

                    r->type = acpi_device_resource_type::spi_serial_bus;
                    memcpy(&r->spi_serial_bus, &res->Data.SpiSerialBus,
                           sizeof(ACPI_RESOURCE_SPI_SERIALBUS));
                    insert_tail_list(d.resources, r->le);
                    break;
                }

                case ACPI_RESOURCE_SERIAL_TYPE_UART: {
                    auto r = new acpi_device_resource_uart_serial_bus;
                    if (!r) {
                        print("out of memory\n");
                        return AE_OK;
                    }

                    r->type = acpi_device_resource_type::uart_serial_bus;
                    memcpy(&r->uart_serial_bus, &res->Data.UartSerialBus,
                           sizeof(ACPI_RESOURCE_UART_SERIALBUS));
                    insert_tail_list(d.resources, r->le);
                    break;
                }

                case ACPI_RESOURCE_SERIAL_TYPE_CSI2: {
                    auto r = new acpi_device_resource_csi2_serial_bus;
                    if (!r) {
                        print("out of memory\n");
                        return AE_OK;
                    }

                    r->type = acpi_device_resource_type::csi2_serial_bus;
                    memcpy(&r->csi2_serial_bus, &res->Data.Csi2SerialBus,
                           sizeof(ACPI_RESOURCE_CSI2_SERIALBUS));
                    insert_tail_list(d.resources, r->le);
                    break;
                }

                default:
                    print("unrecognized ACPI serial bus type {}\n",
                          res->Data.CommonSerialBus.Type);
                    break;
            }

            break;
        }

        // FIXME - ACPI_RESOURCE_TYPE_PIN_FUNCTION
        // FIXME - ACPI_RESOURCE_TYPE_PIN_CONFIG
        // FIXME - ACPI_RESOURCE_TYPE_PIN_GROUP
        // FIXME - ACPI_RESOURCE_TYPE_PIN_GROUP_FUNCTION
        // FIXME - ACPI_RESOURCE_TYPE_PIN_GROUP_CONFIG
        // FIXME - ACPI_RESOURCE_TYPE_CLOCK_INPUT

        default:
            print("FIXME - unhandled ACPI resource type {}\n", res->Type);
            break;
    }

    return AE_OK;
}

static void find_device_resources(acpi_device& d, ACPI_HANDLE obj) {
    ACPI_STATUS status;

    status = AcpiWalkResources(obj, (char*)"_CRS", find_device_resources_cb, &d);

    if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
        print("AcpiWalkResources returned {}\n", status);
        return;
    }
}

static ACPI_STATUS found_device(ACPI_HANDLE Object, uint32_t /*NestingLevel*/,
                                void* /*Context*/, void** /*ReturnValue*/) {
    ACPI_STATUS status;
    char buf[80];
    ACPI_BUFFER name;
    ACPI_DEVICE_INFO* devinfo;

    status = AcpiGetObjectInfo(Object, &devinfo);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetObjectInfo returned {}\n", status);
        return AE_OK;
    }

    string_view hid, uid;
    ACPI_PNP_DEVICE_ID_LIST* cids = nullptr;

    if (devinfo->Valid & ACPI_VALID_HID) {
        hid = string_view(devinfo->HardwareId.String, devinfo->HardwareId.Length);

        if (!hid.empty() && hid.back() == 0)
            hid = string_view(hid.data(), hid.size() - 1);
    }

    if (devinfo->Valid & ACPI_VALID_CID)
        cids = &devinfo->CompatibleIdList;

    if (devinfo->Valid & ACPI_VALID_UID) {
        uid = string_view(devinfo->UniqueId.String, devinfo->UniqueId.Length);

        if (auto nul = uid.find('\0'); nul != string_view::npos)
            uid = string_view(uid.data(), nul);
    }

    name.Pointer = buf;
    name.Length = sizeof(buf);

    status = AcpiGetName(Object, ACPI_FULL_PATHNAME, &name);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetName returned {}\n", status);
        ACPI_FREE(devinfo);
        return AE_OK;
    }

    auto acpi_name = string_view((char*)name.Pointer, name.Length);

    if (!acpi_name.empty() && acpi_name.back() == 0)
        acpi_name = string_view(acpi_name.data(), acpi_name.size() - 1);

    size_t bufsize = hid.size() + uid.size() + acpi_name.size();

    if (cids) {
        for (uint32_t i = 0; i < cids->Count; i++) {
            auto sv = string_view(cids->Ids[i].String, cids->Ids[i].Length);

            if (!sv.empty() && sv.back() == 0)
                sv = string_view(sv.data(), sv.size() - 1);

            bufsize += sv.size();
        }

        bufsize += sizeof(string_view) * cids->Count;
    }

    auto d = (acpi_device*)malloc(offsetof(acpi_device, buf) + bufsize);
    if (!d) {
        print("malloc failed\n");
        ACPI_FREE(devinfo);
        return AE_OK;
    }

    new (d) acpi_device;

    auto ptr = d->buf;

    memcpy(ptr, acpi_name.data(), acpi_name.size());
    d->acpi_name = string_view(ptr, acpi_name.size());
    ptr += acpi_name.size();

    memcpy(ptr, hid.data(), hid.size());
    d->hid = string_view(ptr, hid.size());
    ptr += hid.size();

    memcpy(ptr, uid.data(), uid.size());
    d->uid = string_view(ptr, uid.size());
    ptr += uid.size();

    if (cids) {
        d->cids = span((string_view*)ptr, cids->Count);

        ptr += sizeof(string_view) * cids->Count;

        for (uint32_t i = 0; i < cids->Count; i++) {
            auto sv = string_view(cids->Ids[i].String, cids->Ids[i].Length);

            if (!sv.empty() && sv.back() == 0)
                sv = string_view(sv.data(), sv.size() - 1);

            memcpy(ptr, sv.data(), sv.size());

            d->cids[i] = string_view(ptr, sv.size());

            ptr += sv.size();
        }
    }

    insert_tail_list(acpi_devices, d->le);

    ACPI_FREE(devinfo);

    find_device_resources(*d, Object);

    return AE_OK;
}

export void find_acpi_devices() {
    ACPI_STATUS status;

    status = AcpiGetDevices(nullptr, found_device, nullptr, nullptr);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetDevices returned {}\n", status);
        return;
    }

    // replace unrecognized HIDs with recognized CIDs
    for (auto le = acpi_devices.next; le != &acpi_devices; le = le->next) {
        bool found = false;
        auto& d = containing_record<acpi_device>(le);

        for (auto le = pnp_hids.next; le != &pnp_hids; le = le->next) {
            auto& p = containing_record<acpi_recognizer>(le);

            if (p.hid == d.hid) {
                found = true;
                break;
            }
        }

        if (found)
            continue;

        for (auto c : d.cids) {
            for (auto le = pnp_hids.next; le != &pnp_hids; le = le->next) {
                auto& p = containing_record<acpi_recognizer>(le);

                if (p.hid == c) {
                    found = true;
                    break;
                }
            }

            if (found) {
                d.hid = c;
                break;
            }
        }
    }

    for (auto le = acpi_devices.next; le != &acpi_devices; le = le->next) {
        auto& d = containing_record<acpi_device>(le);

        if (d.hid.empty())
            continue;

        for (auto le = pnp_hids.next; le != &pnp_hids; le = le->next) {
            auto& p = containing_record<acpi_recognizer>(le);

            if (p.hid != d.hid)
                continue;

            if (p.prelim_func)
                p.prelim_func(d.uid);

            break;
        }
    }

    for (auto le = acpi_devices.next; le != &acpi_devices; le = le->next) {
        auto& d = containing_record<acpi_device>(le);

        print("found ACPI device {}, HID {}, UID {}\n",
              d.acpi_name, !d.hid.empty() ? d.hid : "(none)",
              !d.uid.empty() ? d.uid : "(none)");

        if (d.hid.empty())
            continue;

        for (auto le = pnp_hids.next; le != &pnp_hids; le = le->next) {
            auto& p = containing_record<acpi_recognizer>(le);

            if (p.hid != d.hid)
                continue;

            p.func(d);
            break;
        }
    }
}

extern "C"
ACPI_STATUS AcpiOsInitialize() {
    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsTerminate() {
    return AE_OK;
}

extern "C"
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
    return rsdp_pa;
}

extern "C"
ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* /*InitVal*/,
                                     ACPI_STRING* NewVal) {
    *NewVal = nullptr;

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* /*ExistingTable*/,
                                ACPI_TABLE_HEADER** NewTable) {
    *NewTable = nullptr;

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER* /*ExistingTable*/,
                                        ACPI_PHYSICAL_ADDRESS* NewAddress,
                                        UINT32* /*NewTableLength*/) {
    *NewAddress = 0;

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsCreateLock(ACPI_SPINLOCK* OutHandle) {
    *OutHandle = new spinlock;

    return AE_OK;
}

extern "C"
void AcpiOsDeleteLock(ACPI_SPINLOCK Handle) {
    auto* lock = (spinlock*)Handle;

    delete lock;
}

extern "C"
ACPI_CPU_FLAGS AcpiOsAcquireLock(ACPI_SPINLOCK Handle) {
    auto& lock = *(spinlock*)Handle;

    lock.lock();

    return 0;
}

extern "C"
void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS) {
    auto& lock = *(spinlock*)Handle;

    lock.unlock();
}

extern "C"
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
                                  ACPI_SEMAPHORE* OutHandle) {
#ifndef __aarch64__ // FIXME
    auto ref = create_semaphore(MaxUnits);

    auto& sem = static_cast<semaphore&>(ref.get());

    sem.acquire();
    sem.current_count = InitialUnits;

    *OutHandle = &sem;
#endif

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
#ifndef __aarch64__ // FIXME
    auto& sem = *static_cast<semaphore*>(Handle);

    sem.release();
#endif

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units,
                                UINT16 /*Timeout*/) {
    if (!acpi_early_init_done)
        return AE_OK;

#ifndef __aarch64__ // FIXME
    auto& sem = *static_cast<semaphore*>(Handle);

    // FIXME - Timeout

    if (Units != 1) {
        print("FIXME - AcpiOsWaitSemaphore with Units != 1\n"); // FIXME
        return AE_ERROR;
    }

    sem.acquire();
    wait_for_object(sem, true);
    sem.release();
#endif

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
    if (!acpi_early_init_done)
        return AE_OK;

#ifndef __aarch64__ // FIXME
    auto& sem = *static_cast<semaphore*>(Handle);

    if (Units != 1) {
        print("FIXME - AcpiOsSignalSemaphore with Units != 1\n"); // FIXME
        return AE_ERROR;
    }

    release_semaphore(sem);
#endif

    return AE_OK;
}

extern "C"
void* AcpiOsAllocate(ACPI_SIZE Size) {
    return malloc(Size);
}

extern "C"
void AcpiOsFree(void* Memory) {
    free(Memory);
}

extern "C"
void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE) {
    return phys_address(Where);
}

extern "C"
void AcpiOsUnmapMemory(void*, ACPI_SIZE) {
}

extern "C"
ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber,
                                          ACPI_OSD_HANDLER ServiceRoutine,
                                          void* Context) {
#ifndef __aarch64__ // FIXME
    auto h = new acpi_irq_handler;
    if (!h)
        return AE_NO_MEMORY;

    h->service_routine = ServiceRoutine;
    h->context = Context;

    lock_guard lock(irq_handlers_lock);

    for (auto le = irq_handlers.next; le != &irq_handlers; le = le->next) {
        auto& n = containing_record<acpi_irq_num>(le);

        if (n.irq == InterruptNumber) {
            insert_tail_list(n.handlers, h->le);
            return AE_OK;
        }
    }

    auto n = new acpi_irq_num;
    if (!n) {
        delete h;
        return AE_NO_MEMORY;
    }

    n->irq = InterruptNumber;
    insert_tail_list(irq_handlers, n->le);
    insert_tail_list(n->handlers, h->le);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    if (!add_irq_handler(InterruptNumber, local_apic_id, acpi_isr_func, n)) {
        print("add_irq_handler failed\n");
        delete h;
        delete n;

        return AE_ERROR;
    }
#endif

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber,
                                         ACPI_OSD_HANDLER ServiceRoutine) {
#ifndef __aarch64__ // FIXME
    lock_guard lock(irq_handlers_lock);

    for (auto le = irq_handlers.next; le != &irq_handlers; le = le->next) {
        auto& n = containing_record<acpi_irq_num>(le);

        if (n.irq == InterruptNumber) {
            for (auto le2 = n.handlers.next; le2 != &n.handlers; le2 = le2->next) {
                auto& h = containing_record<acpi_irq_handler>(le2);

                if (h.service_routine == ServiceRoutine) {
                    remove_entry_list(h.le);
                    delete &h;

                    // FIXME - if n.handlers now empty, deregister IRQ handler and free n

                    return AE_OK;
                }
            }

            return AE_NOT_FOUND;
        }
    }
#endif

    return AE_NOT_FOUND;
}

// work round ICE in GCC 15
static ACPI_THREAD_ID _AcpiOsGetThreadId() {
    return (ACPI_THREAD_ID)get_current_thread();
}

extern "C"
ACPI_THREAD_ID AcpiOsGetThreadId() {
    return _AcpiOsGetThreadId();
}

extern "C"
ACPI_STATUS AcpiOsExecute(ACPI_EXECUTE_TYPE /*Type*/, ACPI_OSD_EXEC_CALLBACK /*Function*/,
                          void* /*Context*/) {
    print("FIXME - AcpiOsExecute\n");

    return AE_ERROR;
}

extern "C"
void AcpiOsWaitEventsComplete() {
    print("FIXME - AcpiOsWaitEventsComplete\n");
}

extern "C"
void AcpiOsSleep(UINT64 /*Milliseconds*/) {
    print("FIXME - AcpiOsSleep\n");
}

extern "C"
void AcpiOsStall(UINT32 /*Microseconds*/) {
    print("FIXME - AcpiOsStall\n");
}

extern "C"
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS Address, UINT32* Value, UINT32 Width) {
#ifdef __aarch64__ // FIXME?
    return AE_NOT_IMPLEMENTED;
#else
    if (Width == 32)
        *Value = inl((uint16_t)Address);
    else if (Width == 16)
        *Value = inw((uint16_t)Address);
    else if (Width == 8)
        *Value = inb((uint16_t)Address);
    else
        return AE_BAD_PARAMETER;

    return AE_OK;
#endif
}

extern "C"
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
#ifdef __aarch64__ // FIXME?
    return AE_NOT_IMPLEMENTED;
#else
    if (Width == 32)
        outl((uint16_t)Address, Value);
    else if (Width == 16)
        outw((uint16_t)Address, (uint16_t)Value);
    else if (Width == 8)
        outb((uint16_t)Address, (uint8_t)Value);
    else
        return AE_BAD_PARAMETER;

    return AE_OK;
#endif
}

extern "C"
ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS /*Address*/, UINT64* /*Value*/,
                             UINT32 /*Width*/) {
    print("FIXME - AcpiOsReadMemory\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS /*Address*/, UINT64 /*Value*/,
                              UINT32 /*Width*/) {
    print("FIXME - AcpiOsWriteMemory\n");

    return AE_ERROR;
}

static constexpr uint64_t pci_pa(const ACPI_MCFG_ALLOCATION& item, uint8_t bus, uint8_t device, uint8_t function) {
    return item.Address | (uint64_t)((bus - item.StartBusNumber) << 20) | (uint64_t)(device << 15) | (uint64_t)(function << 12);
}

extern "C"
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* PciId, UINT32 Reg,
                                       UINT64* Value, UINT32 Width) {
    ACPI_TABLE_HEADER* tbl;

    auto status = AcpiGetTable((char*)ACPI_SIG_MCFG, 1, &tbl);
    if (ACPI_FAILURE(status))
        return status;

    if (tbl->Length < sizeof(ACPI_TABLE_MCFG))
        return AE_ERROR;

    auto items = span((ACPI_MCFG_ALLOCATION*)((uint8_t*)tbl + sizeof(ACPI_TABLE_MCFG)),
                      (tbl->Length - sizeof(ACPI_TABLE_MCFG)) / sizeof(ACPI_MCFG_ALLOCATION));

    if (items.empty())
        return AE_NOT_FOUND;

    for (auto& it : items) {
        if (it.PciSegment != PciId->Segment)
            continue;

        if (PciId->Bus < it.StartBusNumber || PciId->Bus > it.EndBusNumber)
            continue;

        auto pa = pci_pa(it, (uint8_t)PciId->Bus, (uint8_t)PciId->Device, (uint8_t)PciId->Function) + Reg;
        auto va = phys_address(pa);

        if (Width == 8) {
            *Value = *(volatile uint8_t*)va;
            return AE_OK;
        } else if (Width == 16) {
            *Value = *(volatile uint16_t*)va;
            return AE_OK;
        } else if (Width == 32) {
            *Value = *(volatile uint32_t*)va;
            return AE_OK;
        } else if (Width == 64) {
            *Value = *(volatile uint64_t*)va;
            return AE_OK;
        } else
            return AE_BAD_PARAMETER;
    }

    return AE_NOT_FOUND;
}

extern "C"
ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* /*PciId*/, UINT32 /*Reg*/,
                                        UINT64 /*Value*/, UINT32 /*Width*/) {
    print("FIXME - AcpiOsWritePciConfiguration\n");

    return AE_ERROR;
}

extern "C"
UINT64 AcpiOsGetTimer() {
#ifdef __aarch64__ // FIXME
    return 0;
#else
    // convert to units of 100ns
    return (uint64_t)(system_time * 10000000 / SYSTEM_TIME_RESOLUTION);
#endif
}

extern "C"
ACPI_STATUS AcpiOsSignal(UINT32 /*Function*/, void* /*Info*/) {
    print("FIXME - AcpiOsSignal\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsEnterSleep(UINT8 SleepState, UINT32 RegaValue, UINT32 RegbValue) {
    print("FIXME - AcpiOsEnterSleep({:x}, {:x}, {:x})\n", SleepState, RegaValue, RegbValue);

    return AE_OK;
}

extern "C"
void AcpiOsPrintf(const char* /*Format*/, ...) {
    print("FIXME - AcpiOsPrintf\n");
}
