module;

#include <span>
#include <stdint.h>

export module efi;

import loaderblock;
import bsod;
import config;
import mem;
import print;

using namespace std;

// FIXME
using EFI_MEMORY_DESCRIPTOR = _EFI_MEMORY_DESCRIPTOR;
using EFI_MEMORY_TYPE = _EFI_MEMORY_TYPE;

static const uint64_t UEFI_VABLOCK = 0xfffffa6000000000;
static _EFI_MEMORY_DESCRIPTOR uefi_mappings[256];
export EFI_GET_TIME GetTime = nullptr;

export bool map_uefi(const loader_block& lb) {
    auto map_entries = span((map_entry*)lb.map_ptr, lb.num_map_entries);
    unsigned int count = 0;

    uint64_t va = UEFI_VABLOCK;

    // FIXME - bsod if we would overflow uefi_mappings

    auto d = uefi_mappings;
    for (const auto& m : map_entries) {
        if (m.type == mem_type::uefi_code)
            d->Type = EFI_MEMORY_TYPE::EfiRuntimeServicesCode;
        else if (m.type == mem_type::uefi_data)
            d->Type = EFI_MEMORY_TYPE::EfiRuntimeServicesData;
        else
            continue;

        for (unsigned int i = 0; i < m.len >> PAGE_SHIFT; i++) {
            if (!map_page<true>(m.addr + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT),
                                mapping_type::read_write_execute)) { // FIXME
                print("map_page failed\n");
                return false;
            }
        }

        d->Pad = 0;
        d->PhysicalStart = m.addr;
        d->VirtualStart = va;
        d->NumberOfPages = m.len >> PAGE_SHIFT;
        d->Attribute = 0;

        if (!GetTime && (uintptr_t)lb.GetTime >= m.addr && (uintptr_t)lb.GetTime < m.addr + m.len)
            GetTime = (EFI_GET_TIME)((uintptr_t)lb.GetTime - m.addr + va);

        va += m.len;
        count++;
    }

    if (count == 0)
        bsod("SetVirtualAddressMap set but no UEFI mappings given");

    auto ret = lb.SetVirtualAddressMap(count * sizeof(EFI_MEMORY_DESCRIPTOR),
                                       sizeof(EFI_MEMORY_DESCRIPTOR),
                                       EFI_MEMORY_DESCRIPTOR_VERSION,
                                       uefi_mappings);
    if (ret != 0) { // FIXME - enum
        print("SetVirtualAddressMap returned {}\n", ret);
        return false;
    }

    return true;
}
