#include "kernel.h"
#include "acpi.h"
#include <atomic>
#include <cpuid.h>

using namespace std;

// in smp.S
extern uint8_t ap_trampoline;
extern uint8_t ap_trampoline_end;
extern uint8_t ap_trampoline_cr3;
extern uint64_t ap_trampoline_start_func;

static const uint64_t KERNEL_STACK = 0xfffff90000000000;

static atomic<unsigned int> ap_left;

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static gdt_ptr bsp_gdt, bsp_idt;

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

static void ap_startup() {
    // load GDT and IDT
    // FIXME - do we need to long jump for GDT to take effect?
    __asm __volatile (
        "lgdt %0\n"
        "lidt %1\n"
        :
        : "m" (bsp_gdt), "m" (bsp_idt)
    );

    auto spurious_interrupt_reg = (volatile uint32_t*)&local_apic[0xf0];

    *spurious_interrupt_reg = *spurious_interrupt_reg | 0x100;

    init_lapic_timer(true);

    init_cpu_block();

    ap_left--;

    scheduler(); // doesn't return
}

static uint8_t current_cpu_id() {
    uint32_t eax, ebx, ecx, edx;

    __cpuid(1, eax, ebx, ecx, edx);

    return (uint8_t)((ebx >> 24) & 0xff);
}

void init_smp() {
    auto madt = find_madt();
    if (madt.empty()) {
        print("MADT not found\n");
        return;
    }

    uint8_t bsp_id = current_cpu_id();
    unsigned int ap_count = 0;

    auto data = madt;
    while (data.size() > 2) {
        auto type = (enum madt_type)data[0];
        auto length = data[1];

        if (type == madt_type::local_apic && length >= 2 + sizeof(madt_local_apic)) {
            auto& ent = *(madt_local_apic*)&data[2];

            if (ent.apic_id != bsp_id && (ent.enabled || ent.online_capable)) {
                uint64_t va = KERNEL_STACK | ((ent.apic_id * KERNEL_STACK_PAGES) << PAGE_SHIFT);

                for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
                    auto pa = allocate_page();
                    if (!pa) {
                        print("allocate_page failed\n");
                        // FIXME - deallocate what's already been allocated (or BSOD)
                        return;
                    }

                    if (!map_page(pa, va)) {
                        print("map_page failed\n");
                        // FIXME - deallocate what's already been allocated (or BSOD)
                        return;
                    }

                    va += PAGE_SIZE;
                }

                ap_count++;
            }
        }

        data = data.subspan(length);
    }

    if (ap_count == 0)
        return;

    auto trampoline = (void*)0;

    // we already reserved page 0 in the bootloader
    if (!map_page(0, (uintptr_t)trampoline)) {
        print("map_page failed\n");
        return;
    }

    memcpy(trampoline, &ap_trampoline, &ap_trampoline_end - &ap_trampoline);

    uint32_t* ap_cr3 = (uint32_t*)((uint8_t*)trampoline + (&ap_trampoline_cr3 - &ap_trampoline));
    uint64_t cr3;

    __asm __volatile (
        "mov rax, cr3"
        : "=a" (cr3)
    );

    *ap_cr3 = (uint32_t)cr3; // FIXME - cr3 not guaranteed to be in first 4 GB(?)

    uint64_t* ap_start_func = (uint64_t*)((uint8_t*)trampoline + ((uint8_t*)&ap_trampoline_start_func - &ap_trampoline));

    *ap_start_func = (uintptr_t)ap_startup;

    __asm __volatile (
        "sgdt %0\n"
        "sidt %1\n"
        : "=m" (bsp_gdt), "=m" (bsp_idt)
    );

    ap_left = ap_count;

    data = madt;
    while (data.size() > 2) {
        auto type = (enum madt_type)data[0];
        auto length = data[1];

        if (type == madt_type::local_apic && length >= 2 + sizeof(madt_local_apic)) {
            auto& ent = *(madt_local_apic*)&data[2];

            if (ent.apic_id != bsp_id && (ent.enabled || ent.online_capable)) {
                print("CPU found\n");

                start_ap(ent.apic_id);
            }
        }

        data = data.subspan(length);
    }

    while (ap_left > 0) {
        __asm("pause");
    }

    unmap_page(0);
    // FIXME - now mark PA page 0 as free?
}
