#include "kernel.h"
#include "acpi.h"

static void start_ap(uint8_t i) {
    // FIXME
}

void init_smp() {
    auto madt = find_madt();
    if (madt.empty()) {
        print("MADT not found\n");
        return;
    }

    uint8_t bsp_id;

    __asm __volatile (
        "mov rax, 1\n"
        "cpuid\n"
        "shr ebx, 24\n"
        : "=b" (bsp_id)
        :
        : "rax"
    );

    auto data = madt;
    while (data.size() > 2) {
        auto type = data[0];
        auto length = data[1];

        if (type == 0 && length >= 2 + sizeof(madt_local_apic)) { // processor local APIC
            auto& ent = *(madt_local_apic*)&data[2];

            if (ent.apic_id != bsp_id) {
                print("CPU found\n");
                start_ap(ent.apic_id);
            }
        }

        data = data.subspan(length);
    }
}
