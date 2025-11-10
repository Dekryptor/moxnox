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
import format;

using namespace std;

// FIXME
using EFI_MEMORY_DESCRIPTOR = _EFI_MEMORY_DESCRIPTOR;
using EFI_MEMORY_TYPE = _EFI_MEMORY_TYPE;

static const uint64_t UEFI_VABLOCK = 0xfffffa6000000000;
static _EFI_MEMORY_DESCRIPTOR uefi_mappings[256];
export EFI_GET_TIME GetTime = nullptr;
export EFI_RESET_SYSTEM ResetSystem = nullptr;
export EFI_SET_VARIABLE SetVariable = nullptr;

static const EFI_STATUS EFI_SUCCESS = 0;

extern "C++" void (*_printk)(string_view s);

// print not setup properly by the time map_uefi runs
template<typename... Args>
void do_print(type_identity_t<checker<type_identity_t<Args>...>> fmt, Args... args) {
    char buf[4096];

    auto sp = span(buf, PAGE_SIZE);

    sp = format_to(sp, fmt.sv, make_tuple(args...));

    _printk(string_view(buf, (size_t)(sp.data() - buf)));
}

export bool map_uefi(const loader_block& lb) {
    auto map_entries = span((map_entry*)lb.map_ptr, lb.num_map_entries);
    unsigned int count = 0;

    uint64_t va = UEFI_VABLOCK;

#ifdef __aarch64__
    // work round buggy firmware trying to fixup non-writable code
    mark_lower_half_writable();
#endif

    // FIXME - fail if we would overflow uefi_mappings

    auto d = uefi_mappings;
    for (const auto& m : map_entries) {
        enum mapping_type mt;

        if (!(m.efi_attributes & EFI_MEMORY_RUNTIME))
            continue;

        if (m.type == mem_type::uefi_code)
            d->Type = EFI_MEMORY_TYPE::EfiRuntimeServicesCode;
        else if (m.type == mem_type::uefi_data)
            d->Type = EFI_MEMORY_TYPE::EfiRuntimeServicesData;
        else if (m.type == mem_type::mmio)
            d->Type = EFI_MEMORY_TYPE::EfiMemoryMappedIO;
        else
            continue;

        // FIXME - read-only and NX flags

        if (m.efi_attributes & EFI_MEMORY_WB)
            mt = mapping_type::read_write_execute;
        else if (m.efi_attributes & EFI_MEMORY_UC)
            mt = mapping_type::no_cache;
        else
            continue;

        for (unsigned int i = 0; i < m.len >> PAGE_SHIFT; i++) {
            if (!map_page<true>(m.addr + (i << PAGE_SHIFT), va + (i << PAGE_SHIFT),
                                mt)) {
                do_print("map_page failed\n");
                return false;
            }
        }

        d->Pad = 0;
        d->PhysicalStart = m.addr;
        d->VirtualStart = va;
        d->NumberOfPages = m.len >> PAGE_SHIFT;
        d->Attribute = EFI_MEMORY_RUNTIME;

        if (m.efi_attributes & EFI_MEMORY_UC)
            d->Attribute |= EFI_MEMORY_UC;
        else if (m.efi_attributes & EFI_MEMORY_WB)
            d->Attribute |= EFI_MEMORY_WB;

        if (!GetTime && (uintptr_t)lb.GetTime >= m.addr && (uintptr_t)lb.GetTime < m.addr + m.len)
            GetTime = (EFI_GET_TIME)((uintptr_t)lb.GetTime - m.addr + va);

        if (!ResetSystem && (uintptr_t)lb.ResetSystem >= m.addr && (uintptr_t)lb.ResetSystem < m.addr + m.len)
            ResetSystem = (EFI_RESET_SYSTEM)((uintptr_t)lb.ResetSystem - m.addr + va);

        if (!SetVariable && (uintptr_t)lb.SetVariable >= m.addr && (uintptr_t)lb.SetVariable < m.addr + m.len)
            SetVariable = (EFI_SET_VARIABLE)((uintptr_t)lb.SetVariable - m.addr + va);

        va += m.len;
        count++;
        d++;
    }

    if (count == 0) {
        do_print("SetVirtualAddressMap set but no UEFI mappings given");
        GetTime = nullptr;
        ResetSystem = nullptr;
        return false;
    }

    auto ret = lb.SetVirtualAddressMap(count * sizeof(EFI_MEMORY_DESCRIPTOR),
                                       sizeof(EFI_MEMORY_DESCRIPTOR),
                                       EFI_MEMORY_DESCRIPTOR_VERSION,
                                       uefi_mappings);
    if (ret != EFI_SUCCESS) { // FIXME - enum
        do_print("SetVirtualAddressMap returned {}\n", ret);
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

export void get_time_from_efi() {
    EFI_TIME now;

    auto ret = GetTime(&now, nullptr);
    if (ret != EFI_SUCCESS) { // FIXME - enum
        print("GetTime returned {:x}\n", ret);
        return;
    }

    auto ymd = chrono::year_month_day{chrono::year{now.Year}, chrono::month{now.Month},
                                      chrono::day{now.Day}};

    auto day_num = ymd_to_num(ymd);

    system_time_offset = (uint64_t)((((day_num * 86400) + (now.Hour * 3600) + (now.Minute * 60) + now.Second) * 10000000) - system_time);

    // FIXME - do something with timezone field
}
