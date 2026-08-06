#include <stdint.h>
#include <string.h>
#include <span>
#include "trace.h"

import nt;
import print;
import config;
import mem;
import smp;
import trace;

using namespace std;

static nt_status query_system_basic_info(span<uint8_t> buf, uint32_t& retlen) {
    if (buf.size() < sizeof(SYSTEM_BASIC_INFORMATION))
        return nt_status::BUFFER_TOO_SMALL;

    auto& sbi = *(SYSTEM_BASIC_INFORMATION*)buf.data();

    memset(&sbi, 0, sizeof(SYSTEM_BASIC_INFORMATION));

    // FIXME - this could be all be stuck in the time page

    sbi.TimerResolution = SYSTEM_TIME_RESOLUTION / TICK_RATE;
    sbi.PageSize = PAGE_SIZE;
    sbi.NumberOfPhysicalPages = (uint32_t)num_ram_pages;
    sbi.LowestPhysicalPageNumber = (uint32_t)first_pfn;
    sbi.HighestPhysicalPageNumber = (uint32_t)(total_pages - 1);
    sbi.AllocationGranularity = PAGE_SIZE;
    sbi.MinimumUserModeAddress = 0;
    sbi.MaximumUserModeAddress = ((uint64_t)1 << 47) - 1;
    sbi.ActiveProcessorsAffinityMask = ((uintptr_t)1 << num_cpus) - 1;
    sbi.NumberOfProcessors = (uint8_t)num_cpus;

    retlen = sizeof(SYSTEM_BASIC_INFORMATION);

    return nt_status::SUCCESS;
}

extern "C"
void trace_NtQuerySystemInformation(uintptr_t* args) {
    auto info_class = (SYSTEM_INFORMATION_CLASS)args[0];
    auto info = (void*)args[1];
    auto info_length = (uint32_t)args[2];

    print("NtQuerySystemInformation: info_class = {}, info = {:x}, info_length = {}\n",
          info_class, (uintptr_t)info, info_length);
}

extern "C"
NTSTATUS NtQuerySystemInformation(SYSTEM_INFORMATION_CLASS info_class,
                                  void* info, uint32_t info_length,
                                  uint32_t* return_length) {
    TRACE3("NtQuerySystemInformation", info_class, info, info_length);

    switch (info_class) {
        using enum SYSTEM_INFORMATION_CLASS;

        case BasicInformation:
            return query_system_basic_info(span((uint8_t*)info, info_length),
                                           *return_length);

        default:
            return nt_status::NOT_IMPLEMENTED;
    }
}
