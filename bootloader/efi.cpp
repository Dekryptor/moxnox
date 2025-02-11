#include <efi.h>

extern "C"
EFI_STATUS EFIAPI efi_main(EFI_HANDLE image_handle, EFI_SYSTEM_TABLE* system_table) {
    UINTN key_event = 0;

    system_table->ConOut->OutputString(system_table->ConOut, (CHAR16*)L"Hello world!\r\n");

    system_table->BootServices->WaitForEvent(1, &system_table->ConIn->WaitForKey, &key_event);

    return EFI_SUCCESS;
}

