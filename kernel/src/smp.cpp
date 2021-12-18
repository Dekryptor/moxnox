#include "kernel.h"
#include "acpi.h"

static void start_ap(uint8_t i) {
    // FIXME - should we be disabling interrupts here?

    auto error_status = (volatile uint32_t*)&local_apic[0x280];
    auto icr1 = (volatile uint32_t*)&local_apic[0x300];
    auto icr2 = (volatile uint32_t*)&local_apic[0x310];

    // send INIT IPI

    *error_status = 0;
    *icr2 = (*icr2 & 0x00ffffff) | (i << 24);
    *icr1 = (*icr1 & 0xfff00000) | 0xc500;

    // wait for delivery

    do {
        __asm __volatile ("pause");
    } while (*icr1 & 0x1000);

    // deassert

    *icr2 = (*icr2 & 0x00ffffff) | (i << 24);
    *icr1 = (*icr1 & 0xfff00000) | 0x8500;

    // wait for delivery

    do {
        __asm __volatile ("pause");
    } while (*icr1 & 0x1000);

    pit_delay(PIT_HZ / 100); // wait 10ms

    // send STARTUP IP twice

    for (unsigned int j = 0; j < 2; j++) {
        *error_status = 0;
        *icr2 = (*icr2 & 0x00ffffff) | (i << 24);
        *icr1 = (*icr1 & 0xfff0f800) | 0x000600; // start at 0000:00000000
        pit_delay(PIT_HZ / 5000); // wait 200us

        do {
            __asm __volatile ("pause");
        } while (*icr1 & 0x1000);
    }
}

void init_smp() {
    auto madt = find_madt();
    if (madt.empty()) {
        print("MADT not found\n");
        return;
    }

    uint8_t bsp_id;
    bool trampoline_init = false;

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
        auto type = (enum madt_type)data[0];
        auto length = data[1];

        if (type == madt_type::local_apic && length >= 2 + sizeof(madt_local_apic)) {
            auto& ent = *(madt_local_apic*)&data[2];

            if (ent.apic_id != bsp_id) {
                print("CPU found\n");

                if (!trampoline_init) {
                    auto trampoline = (void const*)0xfffffa3000000000;

                    // we already reserved page 0 in the bootloader
                    if (!map_page(0, (uintptr_t)trampoline)) {
                        print("map_page failed\n");
                        return;
                    }

                    // FIXME - proper trampoline code
                    *(uint64_t*)trampoline = 0xe0ff66c03166; // xor eax, eax ; jmp eax

                    unmap_page((uintptr_t)trampoline);

                    trampoline_init = true;
                }

                start_ap(ent.apic_id);
            }
        }

        data = data.subspan(length);
    }
}
