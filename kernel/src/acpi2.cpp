module;

#include <span>
#include <string_view>
#include <mutex>
#include <string.h>
#include <stdint.h>

module acpi;

#ifdef __x86_64__
import inout;
#endif

import nt;
import print;
import mem;
import thread;
import interrupts;
import disable_interrupts;
import scheduler;
import semaphore;
import object_ref;
import rwsem;
import spinlock;
import config;
import loaderblock;
import efi;
import kernel_time_page;
import gnu_efi;
import evdev_device;
import evdev;
import handle;

using namespace std;

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

class acpi_button : public evdev_device {
public:
    nt_status read(span<uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }
};

static rwsem irq_handlers_lock;
static list_entry irq_handlers = { &irq_handlers, &irq_handlers };

static list_entry acpi_devices = { &acpi_devices, &acpi_devices };

static object_ref power_button;

static void acpi_notify_handler(ACPI_HANDLE device, uint32_t value, void*) {
    print("FIXME - acpi_notify_handler({}, {})\n", (uintptr_t)device, value);
}

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

void init_acpi_early() {
    ACPI_STATUS Status;

    Status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeSubsystem returned {}\n", (acpi_status)Status);
        return;
    }

    Status = AcpiInitializeTables(nullptr, 16, false);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeTables returned {}\n", (acpi_status)Status);
        return;
    }

    Status = AcpiLoadTables();
    if (ACPI_FAILURE(Status)) {
        print("AcpiLoadTables returned {}\n", (acpi_status)Status);
        return;
    }
}

static void power_button_pressed_thread() {
    input_event ev;
    auto& p = static_cast<acpi_button&>(power_button.get());

    ev.type = evdev_type::EV_KEY;
    ev.code = (evdev_code)evdev_key_code::KEY_POWER;
    ev.value = 1;

    p.post_event(ev, true);

    ev.type = evdev_type::EV_KEY;
    ev.code = (evdev_code)evdev_key_code::KEY_POWER;
    ev.value = 0;

    p.post_event(ev, true);
}

static uint32_t power_button_pressed(void*) {
    // interrupts are disabled here
    add_kernel_thread(power_button_pressed_thread);

    return 0;
}

#ifdef __x86_64__
static void call_pic_func() {
    ACPI_OBJECT param;
    ACPI_OBJECT_LIST params;

    param.Type = ACPI_TYPE_INTEGER;
    param.Integer.Value = 1; // APIC mode

    params.Count = 1;
    params.Pointer = &param;

    auto status = AcpiEvaluateObject(nullptr, (char*)"\\_PIC", &params,
                                     nullptr);
    if (ACPI_FAILURE(status) && status != AE_NOT_FOUND) {
        print("AcpiEvaluateObject returned {} for \\_PIC\n", (acpi_status)status);
        return;
    }
}
#endif

void init_acpi() {
    ACPI_STATUS Status;

    Status = AcpiInstallNotifyHandler(ACPI_ROOT_OBJECT, ACPI_SYSTEM_NOTIFY,
                                      acpi_notify_handler, nullptr);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInstallNotifyHandler returned {}\n", (acpi_status)Status);
        return;
    }

    Status = AcpiEnableSubsystem(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE (Status)) {
        print("AcpiEnableSubsystem returned {}\n", (acpi_status)Status);
        return;
    }

    Status = AcpiInitializeObjects(ACPI_FULL_INITIALIZATION);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeObjects returned {}\n", (acpi_status)Status);
        return;
    }

    auto pptr = new acpi_button;
    if (!pptr) {
        print("new failed\n");
        return;
    }

    power_button = object_ref{*pptr};

    // FIXME - put in /dev/input like on Linux
    auto status = name_object(power_button.get(), "powerbutton");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return;
    }

    register_evdev_device(*pptr);

    Status = AcpiInstallFixedEventHandler(ACPI_EVENT_POWER_BUTTON,
                                          power_button_pressed, nullptr);
    if (ACPI_FAILURE(Status)) {
        print("AcpiInstallFixedEventHandler returned {}\n", (acpi_status)Status);
        return;
    }

#ifdef __x86_64__
    call_pic_func();
#endif
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

        case ACPI_RESOURCE_TYPE_ADDRESS16:
        case ACPI_RESOURCE_TYPE_ADDRESS32:
        case ACPI_RESOURCE_TYPE_ADDRESS64: {
            auto r = new acpi_device_resource_address;
            if (!r) {
                print("out of memory\n");
                return AE_OK;
            }

            r->type = acpi_device_resource_type::address;

            auto status = AcpiResourceToAddress64(res, &r->address);
            if (ACPI_FAILURE(status)) {
                print("AcpiResourceToAddress64 returned {}\n", (acpi_status)status);
                delete r;
                return status;
            }

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
        print("AcpiWalkResources returned {}\n", (acpi_status)status);
        return;
    }
}

static void add_device_properties(acpi_device& dev, const ACPI_OBJECT& pkg) {
    if (pkg.Type != ACPI_TYPE_PACKAGE || pkg.Package.Count != 2)
        return;

    auto& name = pkg.Package.Elements[0];
    auto& val = pkg.Package.Elements[1];

    if (name.Type != ACPI_TYPE_STRING)
        return;

    // FIXME - value can also be string, local reference, or package
    if (val.Type == ACPI_TYPE_INTEGER) {
        size_t size = offsetof(acpi_property, buf) + name.String.Length;

        auto p = (acpi_property*)malloc(size); // FIXME?
        if (!p) {
            print("malloc failed\n");
            return;
        }

        new (p) acpi_property;

        memcpy(p->buf, name.String.Pointer, name.String.Length);
        p->name = string_view(p->buf, name.String.Length);
        p->val = val.Integer.Value;

        insert_tail_list(dev.properties, p->le);
    }
}

static void find_device_properties(acpi_device& dev, ACPI_HANDLE Object) {
    ACPI_STATUS status;
    ACPI_BUFFER buf = { ACPI_ALLOCATE_BUFFER, nullptr };

    static const uint8_t device_props_guid[] = { 0x14, 0xd8, 0xff, 0xda, 0xba, 0x6e, 0x8c, 0x4d, 0x8a, 0x91, 0xbc, 0x9b, 0xbf, 0x4a, 0xa3, 0x01 };

    status = AcpiEvaluateObjectTyped(Object, (char*)"_DSD", nullptr, &buf,
                                     ACPI_TYPE_PACKAGE);
    if (ACPI_FAILURE(status)) {
        if (status != AE_NOT_FOUND)
            print("AcpiEvaluateObjectTyped returned {}\n", (acpi_status)status);
        return;
    }

    auto& props = *(ACPI_OBJECT*)buf.Pointer;

    if (props.Type != ACPI_TYPE_PACKAGE || props.Package.Count % 2) {
        AcpiOsFree(buf.Pointer);
        return;
    }

    for (unsigned int i = 0; i < props.Package.Count; i += 2) {
        auto& guid = props.Package.Elements[i];
        auto& properties = props.Package.Elements[i + 1];

        if (guid.Type != ACPI_TYPE_BUFFER || guid.Buffer.Length != 16)
            break;

        if (memcmp(guid.Buffer.Pointer, device_props_guid, sizeof(device_props_guid)))
            continue;

        if (properties.Type != ACPI_TYPE_PACKAGE)
            break;

        for (unsigned int j = 0; j < properties.Package.Count; j++) {
            add_device_properties(dev, properties.Package.Elements[j]);
        }
    }

    AcpiOsFree(buf.Pointer);
}

static ACPI_STATUS found_device(ACPI_HANDLE Object, uint32_t /*NestingLevel*/,
                                void* /*Context*/, void** /*ReturnValue*/) {
    ACPI_STATUS status;
    char buf[80];
    ACPI_BUFFER name;
    ACPI_DEVICE_INFO* devinfo;

    status = AcpiGetObjectInfo(Object, &devinfo);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetObjectInfo returned {}\n", (acpi_status)status);
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
        print("AcpiGetName returned {}\n", (acpi_status)status);
        AcpiOsFree(devinfo);
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
        AcpiOsFree(devinfo);
        return AE_OK;
    }

    new (d) acpi_device;

    d->handle = Object;

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

    AcpiOsFree(devinfo);

    find_device_resources(*d, Object);

    find_device_properties(*d, Object);

    return AE_OK;
}

void find_acpi_devices() {
    ACPI_STATUS status;

    status = AcpiGetDevices(nullptr, found_device, nullptr, nullptr);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetDevices returned {}\n", (acpi_status)status);
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
    if (!acpi_early_init_done) {
        auto ptr = malloc_aligned<true>(sizeof(spinlock), alignof(spinlock));

        *OutHandle = new (ptr) spinlock;
    } else
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
    if (!acpi_early_init_done)
        return 0;

    auto& lock = *(spinlock*)Handle;

    lock.lock();

    return 0;
}

extern "C"
void AcpiOsReleaseLock(ACPI_SPINLOCK Handle, ACPI_CPU_FLAGS) {
    if (!acpi_early_init_done)
        return;

    auto& lock = *(spinlock*)Handle;

    lock.unlock();
}

extern "C"
ACPI_STATUS AcpiOsCreateSemaphore(UINT32 MaxUnits, UINT32 InitialUnits,
                                  ACPI_SEMAPHORE* OutHandle) {
    object_ref ref;

    if (!acpi_early_init_done)
        ref = create_semaphore<true>(MaxUnits);
    else
        ref = create_semaphore<false>(MaxUnits);

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

    sem.acquire();
    wait_for_object(sem, true);
    sem.release();

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
    if (!acpi_early_init_done)
        return malloc_aligned<true>(Size, 1);
    else
        return malloc(Size);
}

extern "C"
void AcpiOsFree(void* Memory) {
    if (!acpi_early_init_done)
        free_no_lock(Memory);
    else
        free(Memory);
}

extern "C"
void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS Where, ACPI_SIZE Length) {
    auto pa = (uint64_t)Where;

    // FIXME - check EFI memory map for whether it should be writethrough, write-combining, etc.

    mem_type t = mem_type::mmio;

    for (auto le = init_mappings.next; le != &init_mappings; le = le->next) {
        const auto& m = containing_record<init_map_entry>(le);

        if (Where >= m.addr && Where < m.addr + m.len) {
            t = m.type;
            break;
        }
    }

    if (t != mem_type::mmio && t != mem_type::reserved)
        return phys_address(Where);

    if (pa & (PAGE_SIZE - 1)) {
        Length += pa & (PAGE_SIZE - 1);
        pa &= ~(PAGE_SIZE - 1);
    }

    if (Length & (PAGE_SIZE - 1))
        Length += PAGE_SIZE - (Length & (PAGE_SIZE - 1));

    if (Length == 0)
        return nullptr;

    auto num_pages = (uint32_t)(Length >> PAGE_SHIFT);
    auto va = allocate_va(num_pages);

    if (!va) {
        print("allocate_va failed\n");
        return nullptr;
    }

    for (unsigned int i = 0; i < num_pages; i++) {
        if (!map_page(pa + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT), mapping_type::no_cache)) {
            print("map_page failed\n");

            for (unsigned int j = 0; j < i; j++) {
                unmap_page(va + (j << PAGE_SHIFT));
            }

            free_va(va, num_pages);

            return nullptr;
        }
    }

    return (void*)(va + (Where & (PAGE_SIZE - 1)));
}

extern "C"
void AcpiOsUnmapMemory(void* LogicalAddress, ACPI_SIZE Size) {
    if (is_phys_memory(LogicalAddress))
        return;

    auto va = (uint64_t)LogicalAddress;

    if (va & (PAGE_SIZE - 1)) {
        Size += va & (PAGE_SIZE - 1);
        va &= ~(PAGE_SIZE - 1);
    }

    if (Size & (PAGE_SIZE - 1))
        Size += PAGE_SIZE - (Size & (PAGE_SIZE - 1));

    if (Size == 0)
        return;

    auto num_pages = (uint32_t)(Size >> PAGE_SHIFT);

    for (unsigned int i = 0; i < num_pages; i++) {
        unmap_page(va + (i << PAGE_SHIFT));
    }

    free_va(va, num_pages);
}

extern "C"
ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 InterruptNumber,
                                          ACPI_OSD_HANDLER ServiceRoutine,
                                          void* Context) {
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

    if (!add_irq_handler(InterruptNumber, false, acpi_isr_func, n)) {
        print("add_irq_handler failed\n");
        delete h;
        delete n;

        return AE_ERROR;
    }

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 InterruptNumber,
                                         ACPI_OSD_HANDLER ServiceRoutine) {
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

#ifdef __x86_64__
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
#else
extern "C"
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS, UINT32*, UINT32) {
    return AE_NOT_IMPLEMENTED;
}
#endif

#ifdef __x86_64__
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
#else
extern "C"
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS, UINT32, UINT32) {
    return AE_NOT_IMPLEMENTED;
}
#endif

extern "C"
ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS /*Address*/, UINT64* /*Value*/,
                             UINT32 /*Width*/) {
    print("FIXME - AcpiOsReadMemory\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS Address, UINT64 Value,
                              UINT32 Width) {
    if (Width != 8 && Width != 16 && Width != 32 && Width != 64)
        return AE_ERROR;

    auto ptr = AcpiOsMapMemory(Address, Width / 8);
    if (!ptr)
        return AE_ERROR;

    switch (Width) {
        case 8:
            *(uint8_t*)ptr = (uint8_t)Value;
            break;

        case 16:
            *(uint16_t*)ptr = (uint16_t)Value;
            break;

        case 32:
            *(uint32_t*)ptr = (uint32_t)Value;
            break;

        case 64:
            *(uint64_t*)ptr = Value;
            break;
    }

    AcpiOsUnmapMemory(ptr, Width / 8);

    return AE_OK;
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
ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* PciId, UINT32 Reg,
                                        UINT64 Value, UINT32 Width) {
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
            *(volatile uint8_t*)va = (uint8_t)Value;
            return AE_OK;
        } else if (Width == 16) {
            *(volatile uint16_t*)va = (uint16_t)Value;
            return AE_OK;
        } else if (Width == 32) {
            *(volatile uint32_t*)va = (uint32_t)Value;
            return AE_OK;
        } else if (Width == 64) {
            *(volatile uint64_t*)va = Value;
            return AE_OK;
        } else
            return AE_BAD_PARAMETER;
    }

    return AE_NOT_FOUND;
}

extern "C"
UINT64 AcpiOsGetTimer() {
    // convert to units of 100ns
    return (uint64_t)(kernel_time_page.system_time * 10000000 / SYSTEM_TIME_RESOLUTION);
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

ACPI_STATUS acpi_evaluate_integer(ACPI_HANDLE handle, ACPI_STRING path,
                                  uint64_t& val) {
    ACPI_OBJECT obj;
    ACPI_BUFFER buf;

    buf.Length = sizeof(obj);
    buf.Pointer = &obj;

    auto status = AcpiEvaluateObject(handle, path, nullptr, &buf);
    if (ACPI_FAILURE(status))
        return status;

    if (obj.Type != ACPI_TYPE_INTEGER)
        return AE_BAD_DATA;

    val = obj.Integer.Value;

    return AE_OK;
}
