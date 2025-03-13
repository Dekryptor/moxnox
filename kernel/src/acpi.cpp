module;

#include <span>
#include <string_view>
#include "acpica.h"
#include "kernel.h"
#include "handle.h"
#include "objects.h"
#include "semaphore.h"

export module acpi;

import format;
import print;
import inout;

using namespace std;

struct acpi_irq_handler {
    ACPI_OSD_HANDLER service_routine;
    void* context;
    list_entry le;
};

static spinlock irq_handlers_lock;
static list_entry irq_handlers = { &irq_handlers, &irq_handlers };

static const uint8_t ACPI_IRQ = 9; // FIXME - don't hardcode

static const uint8_t acpi_interrupt = 0x25;

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

export void init_acpi() {
    ACPI_STATUS Status;

    add_idt_entry(acpi_interrupt, (void*)acpi_isr, false, 0, false);

    auto local_apic_id = *(volatile uint32_t*)&local_apic[0x20];

    add_irq_handler(ACPI_IRQ, acpi_interrupt, local_apic_id);

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

extern "C"
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID* /*PciId*/, UINT32 /*Reg*/,
                                       UINT64* /*Value*/, UINT32 /*Width*/) {
    print("FIXME - AcpiOsReadPciConfiguration\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID* /*PciId*/, UINT32 /*Reg*/,
                                        UINT64 /*Value*/, UINT32 /*Width*/) {
    print("FIXME - AcpiOsWritePciConfiguration\n");

    return AE_ERROR;
}

extern "C"
UINT64 AcpiOsGetTimer() {
    print("FIXME - AcpiOsGetTimer\n");

    return 0;
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
