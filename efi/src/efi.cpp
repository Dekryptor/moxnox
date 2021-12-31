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
#include <string_view>
#include <span>
#include "partinfo.h"

using namespace std;

static EFI_SYSTEM_TABLE* systable;
static EFI_BOOT_SERVICES* bs;

static const EFI_GUID moxnox_partition_guid = { 0x5dd71063, 0x95c9, 0x41b2, { 0x82, 0x52, 0x84, 0x13, 0xfe, 0xac, 0x64, 0x90 } };

static void print(const char16_t* s) {
    systable->ConOut->OutputString(systable->ConOut, (CHAR16*)s);
}

static span<char16_t> sp_wcscpy(span<char16_t> dest, u16string_view src) {
    auto to_copy = min(src.size(), dest.size());

    memcpy(dest.data(), src.data(), to_copy * sizeof(char16_t));

    return dest.subspan(to_copy);
}

const u16string_view error_string(EFI_STATUS Status) {
    switch (Status) {
        case EFI_SUCCESS:
            return u"EFI_SUCCESS";

        case EFI_LOAD_ERROR:
            return u"EFI_LOAD_ERROR";

        case EFI_INVALID_PARAMETER:
            return u"EFI_INVALID_PARAMETER";

        case EFI_UNSUPPORTED:
            return u"EFI_UNSUPPORTED";

        case EFI_BAD_BUFFER_SIZE:
            return u"EFI_BAD_BUFFER_SIZE";

        case EFI_BUFFER_TOO_SMALL:
            return u"EFI_BUFFER_TOO_SMALL";

        case EFI_NOT_READY:
            return u"EFI_NOT_READY";

        case EFI_DEVICE_ERROR:
            return u"EFI_DEVICE_ERROR";

        case EFI_WRITE_PROTECTED:
            return u"EFI_WRITE_PROTECTED";

        case EFI_OUT_OF_RESOURCES:
            return u"EFI_OUT_OF_RESOURCES";

        case EFI_VOLUME_CORRUPTED:
            return u"EFI_VOLUME_CORRUPTED";

        case EFI_VOLUME_FULL:
            return u"EFI_VOLUME_FULL";

        case EFI_NO_MEDIA:
            return u"EFI_NO_MEDIA";

        case EFI_MEDIA_CHANGED:
            return u"EFI_MEDIA_CHANGED";

        case EFI_NOT_FOUND:
            return u"EFI_NOT_FOUND";

        case EFI_ACCESS_DENIED:
            return u"EFI_ACCESS_DENIED";

        case EFI_NO_RESPONSE:
            return u"EFI_NO_RESPONSE";

        case EFI_NO_MAPPING:
            return u"EFI_NO_MAPPING";

        case EFI_TIMEOUT:
            return u"EFI_TIMEOUT";

        case EFI_NOT_STARTED:
            return u"EFI_NOT_STARTED";

        case EFI_ALREADY_STARTED:
            return u"EFI_ALREADY_STARTED";

        case EFI_ABORTED:
            return u"EFI_ABORTED";

        case EFI_ICMP_ERROR:
            return u"EFI_ICMP_ERROR";

        case EFI_TFTP_ERROR:
            return u"EFI_TFTP_ERROR";

        case EFI_PROTOCOL_ERROR:
            return u"EFI_PROTOCOL_ERROR";

        case EFI_INCOMPATIBLE_VERSION:
            return u"EFI_INCOMPATIBLE_VERSION";

        case EFI_SECURITY_VIOLATION:
            return u"EFI_SECURITY_VIOLATION";

        case EFI_CRC_ERROR:
            return u"EFI_CRC_ERROR";

        case EFI_END_OF_MEDIA:
            return u"EFI_END_OF_MEDIA";

        case EFI_END_OF_FILE:
            return u"EFI_END_OF_FILE";

        case EFI_INVALID_LANGUAGE:
            return u"EFI_INVALID_LANGUAGE";

        case EFI_COMPROMISED_DATA:
            return u"EFI_COMPROMISED_DATA";

        default:
            return u"(unknown error)";
    }
}

void print_error(u16string_view func, EFI_STATUS Status) {
    char16_t s[255];
    auto sp = span(s, (sizeof(s) / sizeof(char16_t)) - 1);

    sp = sp_wcscpy(sp, func);
    sp = sp_wcscpy(sp, u" returned ");
    sp = sp_wcscpy(sp, error_string(Status));
    sp = sp_wcscpy(sp, u"\r\n");

    *sp.data() = 0;
    print(s);
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
        print_error(u"scan_partitions", status);
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
