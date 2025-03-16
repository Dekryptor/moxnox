module;

#include <span>
#include <string_view>
#include "acpica.h"
#include "kernel.h"
#include "handle.h"
#include "objects.h"
#include "semaphore.h"
#include "tick_rate.h"

export module acpi;

import format;
import print;
import inout;
import rtc;
import keyboard;
import mem;
import list;

using namespace std;

struct acpi_irq_handler {
    ACPI_OSD_HANDLER service_routine;
    void* context;
    list_entry le;
};

static spinlock irq_handlers_lock;
static list_entry irq_handlers = { &irq_handlers, &irq_handlers };

static const uint8_t ACPI_IRQ = 9; // FIXME - don't hardcode

static const uint8_t acpi_interrupt = 0x30;

struct acpi_device {
    list_entry le;
    string_view acpi_name;
    string_view hid;
    list_entry resources = { &resources, &resources };
    char buf[0];
};

enum class acpi_device_resource_type {
    io,
    irq,
    extended_irq,
    address16,
    address32,
    address64,
    fixed_memory32,
    dma
};

struct acpi_device_resource {
    list_entry le;
    enum acpi_device_resource_type type;
};

struct acpi_device_resource_io : acpi_device_resource {
    ACPI_RESOURCE_IO io;
};

struct acpi_device_resource_irq : acpi_device_resource {
    ACPI_RESOURCE_IRQ irq;
};

struct acpi_device_resource_extended_irq : acpi_device_resource {
    ACPI_RESOURCE_EXTENDED_IRQ extended_irq;
};

struct acpi_device_resource_address16 : acpi_device_resource {
    ACPI_RESOURCE_ADDRESS16 address16;
};

struct acpi_device_resource_address32 : acpi_device_resource {
    ACPI_RESOURCE_ADDRESS32 address32;
};

struct acpi_device_resource_address64 : acpi_device_resource {
    ACPI_RESOURCE_ADDRESS64 address64;
};

struct acpi_device_resource_fixed_memory32 : acpi_device_resource {
    ACPI_RESOURCE_FIXED_MEMORY32 fixed_memory32;
};

struct acpi_device_resource_dma : acpi_device_resource {
    ACPI_RESOURCE_DMA dma;
};

static list_entry acpi_devices = { &acpi_devices, &acpi_devices };

static void found_rtc(const acpi_device& d);
static void found_ps2_keyboard(const acpi_device& d);

static const struct {
    string_view hid;
    void (*func)(const acpi_device&);
} pnp_hids[] = {
    { "PNP0B00", found_rtc },
    { "PNP0300", found_ps2_keyboard },
    { "PNP0301", found_ps2_keyboard },
    { "PNP0302", found_ps2_keyboard },
    { "PNP0303", found_ps2_keyboard },
    { "PNP0304", found_ps2_keyboard },
    { "PNP0305", found_ps2_keyboard },
    { "PNP0306", found_ps2_keyboard },
    { "PNP0309", found_ps2_keyboard },
    { "PNP030A", found_ps2_keyboard },
    { "PNP030B", found_ps2_keyboard },
    { "PNP0320", found_ps2_keyboard },
    { "PNP0343", found_ps2_keyboard },
    { "PNP0344", found_ps2_keyboard },
    { "PNP0345", found_ps2_keyboard },
    { "CPQA0D7", found_ps2_keyboard },
    { "", nullptr }
};

export bool acpi_early_init_done = false;

extern "C" void acpi_isr();

static void acpi_notify_handler(ACPI_HANDLE device, uint32_t value, void*) {
    print("FIXME - acpi_notify_handler({}, {})\n", (uintptr_t)device, value);
}

extern "C"
void acpi_isr_func() {
    lock_guard lock(irq_handlers_lock);

    for (auto le = irq_handlers.next; le != &irq_handlers; le = le->next) {
        auto& h = containing_record<acpi_irq_handler>(le);

        if (h.service_routine(h.context) == ACPI_INTERRUPT_HANDLED)
            break;
    }
}

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

export void init_acpi() {
    ACPI_STATUS Status;

    add_idt_entry(acpi_interrupt, (void*)acpi_isr, false, 0, false);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    add_irq_handler(ACPI_IRQ, acpi_interrupt, local_apic_id);

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
}

static void found_rtc(const acpi_device& d) {
    optional<uint16_t> port;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        if (r.type != acpi_device_resource_type::io)
            continue;

        auto& io = *reinterpret_cast<acpi_device_resource_io*>(&r);

        port = io.io.Minimum;
        break;
    }

    if (!port.has_value()) {
        print("no I/O port found for RTC\n");
        return;
    }

    init_rtc(port.value());
}

static void found_ps2_keyboard(const acpi_device& d) {
    optional<uint16_t> port1, port2;

    for (auto le = d.resources.next; le != &d.resources; le = le->next) {
        auto& r = containing_record<acpi_device_resource>(le);

        if (r.type == acpi_device_resource_type::io) {
            auto& r2 = static_cast<acpi_device_resource_io&>(r);

            if (r2.io.IoDecode == ACPI_DECODE_16) {
                if (port1.has_value() && !port2.has_value())
                    port2 = r2.io.Minimum;
                else if (!port1.has_value())
                    port1 = r2.io.Minimum;
            }
        }
    }

    if (!port1.has_value() || !port2.has_value())
        return;

    // FIXME - also pass through IRQ no.
    init_keyboard(*port1, *port2);
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

        case ACPI_RESOURCE_TYPE_END_TAG:
            break;

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

    string_view hid;

    if (devinfo->Valid & ACPI_VALID_HID) {
        hid = string_view(devinfo->HardwareId.String, devinfo->HardwareId.Length);

        if (!hid.empty() && hid.back() == 0)
            hid = string_view(hid.data(), hid.size() - 1);
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

    size_t bufsize = hid.size() + acpi_name.size();

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

    for (auto le = acpi_devices.next; le != &acpi_devices; le = le->next) {
        auto& d = containing_record<acpi_device>(le);

        print("found ACPI device {}, HID {}\n", d.acpi_name, !d.hid.empty() ? d.hid : "(none)");

        for (unsigned int i = 0; !pnp_hids[i].hid.empty(); i++) {
            auto& p = pnp_hids[i];

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
    auto ref = create_semaphore(MaxUnits);

    auto& sem = static_cast<semaphore&>(ref.get());

    sem.acquire();
    sem.current_count = InitialUnits;

    *OutHandle = &sem;

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsDeleteSemaphore(ACPI_SEMAPHORE Handle) {
    auto& sem = *static_cast<semaphore*>(Handle);

    sem.release();

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units,
                                UINT16 /*Timeout*/) {
    if (!acpi_early_init_done)
        return AE_OK;

    auto& sem = *static_cast<semaphore*>(Handle);

    // FIXME - Timeout

    if (Units != 1) {
        print("FIXME - AcpiOsWaitSemaphore with Units != 1\n"); // FIXME
        return AE_ERROR;
    }

    wait_for_object(sem, true);

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE Handle, UINT32 Units) {
    if (!acpi_early_init_done)
        return AE_OK;

    auto& sem = *static_cast<semaphore*>(Handle);

    if (Units != 1) {
        print("FIXME - AcpiOsSignalSemaphore with Units != 1\n"); // FIXME
        return AE_ERROR;
    }

    release_semaphore(sem);

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
    if (InterruptNumber != ACPI_IRQ) { // FIXME
        print("FIXME - AcpiOsInstallInterruptHandler with unusual InterruptNumber ({}, expected {})\n", InterruptNumber, ACPI_IRQ);
        return AE_ERROR;
    }

    auto h = new acpi_irq_handler;
    if (!h)
        return AE_NO_MEMORY;

    h->service_routine = ServiceRoutine;
    h->context = Context;

    {
        lock_guard lock(irq_handlers_lock);
        insert_tail_list(irq_handlers, h->le);
    }

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 /*InterruptNumber*/,
                                         ACPI_OSD_HANDLER /*ServiceRoutine*/) {
    print("FIXME - AcpiOsRemoveInterruptHandler\n");

    return AE_ERROR;
}

extern "C"
ACPI_THREAD_ID AcpiOsGetThreadId() {
    return (ACPI_THREAD_ID)get_current_thread();
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
    if (Width == 32)
        *Value = inl((uint16_t)Address);
    else if (Width == 16)
        *Value = inw((uint16_t)Address);
    else if (Width == 8)
        *Value = inb((uint16_t)Address);
    else
        return AE_BAD_PARAMETER;

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS Address, UINT32 Value, UINT32 Width) {
    if (Width == 32)
        outl((uint16_t)Address, Value);
    else if (Width == 16)
        outw((uint16_t)Address, (uint16_t)Value);
    else if (Width == 8)
        outb((uint16_t)Address, (uint8_t)Value);
    else
        return AE_BAD_PARAMETER;

    return AE_OK;
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
    // convert to units of 100ns
    return system_time * 10000000 / SYSTEM_TIME_RESOLUTION;
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
