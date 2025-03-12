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

export void acpi_test() {
    ACPI_STATUS Status;

    Status = AcpiInitializeSubsystem();
    if (ACPI_FAILURE(Status)) {
        print("AcpiInitializeSubsystem returned {}\n", Status);
        return;
    }

    // FIXME
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
    print("FIXME - AcpiOsGetRootPointer\n");

    return 0;
}

extern "C"
ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES* /*InitVal*/,
                                     ACPI_STRING* /*NewVal*/) {
    print("FIXME - AcpiOsPredefinedOverride\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER* /*ExistingTable*/,
                                ACPI_TABLE_HEADER** /*NewTable*/) {
    print("FIXME - AcpiOsTableOverride\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER* /*ExistingTable*/,
                                        ACPI_PHYSICAL_ADDRESS* /*NewAddress*/,
                                        UINT32* /*NewTableLength*/) {
    print("FIXME - AcpiOsPhysicalTableOverride\n");

    return AE_ERROR;
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
ACPI_STATUS AcpiOsWaitSemaphore(ACPI_SEMAPHORE Handle, UINT32 /*Units*/,
                                UINT16 /*Timeout*/) {
    auto& sem = *static_cast<semaphore*>(Handle);

    // FIXME - Timeout
    // FIXME - Units != 1

    wait_for_object(sem, true);

    return AE_OK;
}

extern "C"
ACPI_STATUS AcpiOsSignalSemaphore(ACPI_SEMAPHORE /*Handle*/, UINT32 /*Units*/) {
    print("FIXME - AcpiOsSignalSemaphore\n");

    return AE_ERROR;
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
void* AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS /*Where*/, ACPI_SIZE /*Length*/) {
    print("FIXME - AcpiOsMapMemory\n");

    return nullptr;
}

extern "C"
void AcpiOsUnmapMemory(void* /*LogicalAddress*/, ACPI_SIZE /*Size*/) {
    print("FIXME - AcpiOsUnmapMemory\n");
}

extern "C"
ACPI_STATUS AcpiOsInstallInterruptHandler(UINT32 /*InterruptNumber*/,
                                          ACPI_OSD_HANDLER /*ServiceRoutine*/,
                                          void* /*Context*/) {
    print("FIXME - AcpiOsInstallInterruptHandler\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsRemoveInterruptHandler(UINT32 /*InterruptNumber*/,
                                         ACPI_OSD_HANDLER /*ServiceRoutine*/) {
    print("FIXME - AcpiOsRemoveInterruptHandler\n");

    return AE_ERROR;
}

extern "C"
ACPI_THREAD_ID AcpiOsGetThreadId() {
    print("FIXME - AcpiOsGetThreadId\n");

    return 0;
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
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS /*Address*/, UINT32* /*Value*/, UINT32 /*Width*/) {
    print("FIXME - AcpiOsReadPort\n");

    return AE_ERROR;
}

extern "C"
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS /*Address*/, UINT32 /*Value*/, UINT32 /*Width*/) {
    print("FIXME - AcpiOsWritePort\n");

    return AE_ERROR;
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
void AcpiOsPrintf(const char* /*Format*/, ...) {
    print("FIXME - AcpiOsPrintf\n");
}
