module;

#include <span>
#include <chrono>
#include <stdint.h>

export module efi;

import loaderblock;
import bsod;
import config;
import mem;
import print;
import timer;

using namespace std;

// FIXME
using EFI_MEMORY_DESCRIPTOR = _EFI_MEMORY_DESCRIPTOR;
using EFI_MEMORY_TYPE = _EFI_MEMORY_TYPE;

static const uint64_t UEFI_VABLOCK = 0xfffffa6000000000;
static _EFI_MEMORY_DESCRIPTOR uefi_mappings[256];
export EFI_GET_TIME GetTime = nullptr;

static const EFI_STATUS EFI_SUCCESS = 0;

export bool map_uefi(const loader_block& lb) {
    auto map_entries = span((map_entry*)lb.map_ptr, lb.num_map_entries);
    unsigned int count = 0;

    uint64_t va = UEFI_VABLOCK;

    // FIXME - bsod if we would overflow uefi_mappings

    auto d = uefi_mappings;
    for (const auto& m : map_entries) {
        enum mapping_type mt;

        if (m.type == mem_type::uefi_code) {
            d->Type = EFI_MEMORY_TYPE::EfiRuntimeServicesCode;
            mt = mapping_type::read_execute;
        } else if (m.type == mem_type::uefi_data) {
            d->Type = EFI_MEMORY_TYPE::EfiRuntimeServicesData;
            mt = mapping_type::read_write;
        } else if (m.type == mem_type::mmio) {
            d->Type = EFI_MEMORY_TYPE::EfiMemoryMappedIO;
            mt = mapping_type::no_cache;
        } else
            continue;

        for (unsigned int i = 0; i < m.len >> PAGE_SHIFT; i++) {
            if (!map_page<true>(m.addr + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT),
                                mt)) {
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
        d++;
    }

    if (count == 0)
        bsod("SetVirtualAddressMap set but no UEFI mappings given");

    auto ret = lb.SetVirtualAddressMap(count * sizeof(EFI_MEMORY_DESCRIPTOR),
                                       sizeof(EFI_MEMORY_DESCRIPTOR),
                                       EFI_MEMORY_DESCRIPTOR_VERSION,
                                       uefi_mappings);
    if (ret != EFI_SUCCESS) { // FIXME - enum
        print("SetVirtualAddressMap returned {}\n", ret);
        return false;
    }

    return true;
}

export constexpr int64_t ymd_to_num(const chrono::year_month_day& d) noexcept {
    int m2 = ((int)(unsigned int)d.month() - 14) / 12;
    int64_t n;

    n = (1461 * ((int)d.year() + 4800 + m2)) / 4;
    n += (367 * ((int)(unsigned int)d.month() - 2 - (12 * m2))) / 12;
    n -= (3 * (((int)d.year() + 4900 + m2)/100)) / 4;
    n += (unsigned int)d.day();
    n -= 2337889;

    return n;
}

export void set_time_from_efi() {
    EFI_TIME now;

    auto ret = GetTime(&now, nullptr);
    if (ret != EFI_SUCCESS) { // FIXME - enum
        print("GetTime returned {}\n", ret);
        return;
    }

    auto ymd = chrono::year_month_day{chrono::year{now.Year}, chrono::month{now.Month},
                                      chrono::day{now.Day}};

    auto day_num = ymd_to_num(ymd);

    system_time_offset = (uint64_t)((((day_num * 86400) + (now.Hour * 3600) + (now.Minute * 60) + now.Second) * 10000000) - system_time);

    // FIXME - do something with timezone field
}
