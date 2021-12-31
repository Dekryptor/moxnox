#include <efibind.h>
#include <efidef.h>
#include <efierr.h>
#include <efidevp.h>
#include <efiprot.h>
#include <eficon.h>
#include <efiapi.h>
#include <efipart.h>
#include <efigpt.h>
#include <string.h>
#include "partinfo.h"

static EFI_SYSTEM_TABLE* systable;
static EFI_BOOT_SERVICES* bs;

static const EFI_GUID moxnox_partition_guid = { 0x5dd71063, 0x95c9, 0x41b2, { 0x82, 0x52, 0x84, 0x13, 0xfe, 0xac, 0x64, 0x90 } };

static void print(const char16_t* s) {
    systable->ConOut->OutputString(systable->ConOut, (CHAR16*)s);
}

static EFI_STATUS scan_partitions(EFI_HANDLE image_handle) {
    EFI_GUID guid = EFI_PARTITION_INFO_PROTOCOL_GUID;
    EFI_HANDLE* handles = nullptr;
    UINTN count;
    EFI_STATUS Status;

    Status = bs->LocateHandleBuffer(ByProtocol, &guid, nullptr, &count, &handles);
    if (EFI_ERROR(Status))
        return Status;

    for (unsigned int i = 0; i < count; i++) {
        EFI_PARTITION_INFO_PROTOCOL* pip = nullptr;

        Status = bs->OpenProtocol(handles[i], &guid, (void**)&pip, image_handle, nullptr,
                                  EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
        if (EFI_ERROR(Status))
            continue;

        if (pip->Revision != EFI_PARTITION_INFO_PROTOCOL_REVISION) {
            print(u"unexpected Revision value for EFI_PARTITION_INFO_PROTOCOL\n");
            goto next;
        }

        if (pip->Type != PARTITION_TYPE_GPT)
            goto next;

        if (memcmp(&pip->Info.Gpt.PartitionTypeGUID, &moxnox_partition_guid, sizeof(EFI_GUID)))
            goto next;

        // FIXME
        print(u"FIXME - GPT\r\n");

next:
        bs->CloseProtocol(handles[i], &guid, image_handle, nullptr);
    }

    bs->FreePool(handles);

    if (count == 0)
        print(u"No partitions found.\r\n");

    return EFI_NOT_FOUND;
}

static void wait_for_key() {
    UINTN event;

    bs->WaitForEvent(1, &systable->ConIn->WaitForKey, &event);
}

extern "C"
EFI_STATUS EFIAPI efi_main(EFI_HANDLE ImageHandle, EFI_SYSTEM_TABLE* SystemTable) {
    EFI_STATUS status;

    systable = SystemTable;
    bs = systable->BootServices;

    // FIXME - load drivers

    status = scan_partitions(ImageHandle);
    if (status != EFI_SUCCESS) {
        print(u"scan_partitions failed\r\n");
        goto end;
    }
    // FIXME - if multiple found, show list

    // FIXME - load kernel into memory

    // FIXME - parse memory map
    // FIXME - create loader block

    // FIXME - change CR3, and remap boot services
    // FIXME - exit boot services
    // FIXME - set NX bit etc. (check spec for what is set for us)
    // FIXME - stack
    // FIXME - GDT?
    // FIXME - jump to entry point


    // FIXME - GOP
    // FIXME - pass runtime services list through to kernel

    print(u"Lemon curry?\n");

end:
    wait_for_key();

    return EFI_SUCCESS;
}
