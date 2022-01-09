#include <stdint.h>
#include "../../kernel/src/pe.h"

extern "C"
bool DllMainCRTStartup(uintptr_t, uint32_t, void*) {
    return true;
}

extern "C" __declspec(dllexport)
void mn_start_process(void* img_base) {
    auto& dos_header = *(IMAGE_DOS_HEADER*)img_base;
    auto& nt_header = *(IMAGE_NT_HEADERS*)((uint8_t*)img_base + dos_header.e_lfanew);

    // FIXME - load DLLs (recursively)
    // FIXME - resolve imports
    // FIXME - relocations

    uint32_t entry_point;

    if (nt_header.OptionalHeader32.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        entry_point = nt_header.OptionalHeader64.AddressOfEntryPoint;
    else
        entry_point = nt_header.OptionalHeader32.AddressOfEntryPoint;

    __asm __volatile(
        "jmp %0"
        :
        : "a" ((uint8_t*)img_base + entry_point)
    );
}
