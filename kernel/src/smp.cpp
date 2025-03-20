module;

#include "kernel.h"
#include "acpica.h"
#include "cpu_block.h"
#include "mem.h"
#include <atomic>

export module smp;

import print;
import mem;
import config;
import interrupts;
import timer;
import syscalls;

using namespace std;

// in smp.S
extern "C" uint8_t ap_trampoline;
extern "C" uint8_t ap_trampoline_end;
extern "C" uint8_t ap_trampoline_cr3;
extern "C" uint64_t ap_trampoline_start_func;

static const uint64_t KERNEL_STACK = 0xfffff90000000000;

static atomic<unsigned int> ap_left;

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static gdt_ptr bsp_gdt, bsp_idt;

export uint8_t* apic_ids;
export unsigned int num_cpus;

static void start_ap(uint8_t i) {
    // FIXME - should we be disabling interrupts here?

    auto error_status = (volatile uint32_t*)&local_apic[0x280];
    auto icr1 = (volatile uint32_t*)&local_apic[0x300];
    auto icr2 = (volatile uint32_t*)&local_apic[0x310];

    // send INIT IPI

    *error_status = 0;
    *icr2 = (*icr2 & 0x00ffffff) | (uint32_t)(i << 24);
    *icr1 = (*icr1 & 0xfff00000) | 0xc500;

    // wait for delivery

    do {
        __asm __volatile ("pause");
    } while (*icr1 & 0x1000);

    // deassert

    *icr2 = (*icr2 & 0x00ffffff) | (uint32_t)(i << 24);
    *icr1 = (*icr1 & 0xfff00000) | 0x8500;

    // wait for delivery

    do {
        __asm __volatile ("pause");
    } while (*icr1 & 0x1000);

    pit_delay(PIT_HZ / 100); // wait 10ms

    // send STARTUP IP twice

    for (unsigned int j = 0; j < 2; j++) {
        *error_status = 0;
        *icr2 = (*icr2 & 0x00ffffff) | (uint32_t)(i << 24);
        *icr1 = (*icr1 & 0xfff0f800) | 0x000600; // start at 0000:00000000
        pit_delay(PIT_HZ / 5000); // wait 200us

        do {
            __asm __volatile ("pause");
        } while (*icr1 & 0x1000);
    }
}

export uint8_t current_cpu_id() {
    uint32_t eax, ebx, ecx, edx;

    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (1)
    );

    return (uint8_t)((ebx >> 24) & 0xff);
}

static void setup_ap_gdt(task_state_segment* tss) {
    auto& cb = *get_cpu_block();

    // FIXME - BSOD if any of this fails

    auto pa = allocate_page();
    if (!pa) {
        print("allocate_page failed\n");
        return;
    }

    auto va = allocate_va(1);
    if (!va) {
        print("allocate_va failed\n");
        return;
    }

    if (!map_page(pa, va, mapping_type::read_write)) {
        print("map_page failed\n");
        return;
    }

    cb.gdt = (void*)va;
    memcpy(cb.gdt, (void*)bsp_gdt.base, bsp_gdt.limit + 1);

    auto gdtarr = span((gdt_entry*)cb.gdt, 5);

    // TSS
    gdtarr[3].LimitLow = (uint16_t)sizeof(task_state_segment);
    gdtarr[3].BaseLow = ((uintptr_t)tss & 0xffff);
    gdtarr[3].BaseMid = ((uintptr_t)tss >> 16) & 0xff;
    gdtarr[3].Type = 0x9;
    gdtarr[3].Dpl = 0;
    gdtarr[3].Pres = 1;
    gdtarr[3].LimitHi = 0x0;
    gdtarr[3].Sys = 0;
    gdtarr[3].Long = 0;
    gdtarr[3].Default_Big = 0;
    gdtarr[3].Granularity = 0;
    gdtarr[3].BaseHi = ((uintptr_t)tss >> 24) & 0xff;

    *(uint64_t*)&gdtarr[4] = (uintptr_t)tss >> 32;

    gdt_ptr gp;

    gp.base = (uint64_t)cb.gdt;
    gp.limit = bsp_gdt.limit;

    __asm__ __volatile__ (
        "lgdt %0\n"
        "push 0x8\n"
        "lea rax, [rip+label]\n"
        "push rax\n"
        "retfq\n"
        "label:\n"
        :
        : "m" (gp)
        : "rax"
    );

    __asm__ __volatile__ (
        "mov ax, %0\n"
        "ltr ax\n"
        :
        : "i" (TSS_SEGMENT)
        : "ax"
    );
}

static void ap_startup() {
    // load IDT
    __asm __volatile (
        "lidt %0\n"
        :
        : "m" (bsp_idt)
    );

    // disable 8259 PIC
    __asm __volatile (
        "mov al, 0xff\n"
        "out 0xa1, al\n"
        "out 0x21, al\n"
        :
        :
        : "al"
    );

    auto spurious_interrupt_reg = (volatile uint32_t*)&local_apic[0xf0];

    *spurious_interrupt_reg = spurious_interrupt | 0x100;

    auto apic_id = current_cpu_id();

    init_cpu_block(apic_id);

    setup_ap_gdt(init_tss());

    init_pat();

    init_syscalls();

    init_lapic_timer(true);

    auto num = ap_left.fetch_sub(1, memory_order_acq_rel);

    apic_ids[num_cpus - num] = apic_id;

    acquire_sync_lock();

    __asm __volatile ( // doesn't return
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (GS_SCHEDULER_STACK)
    );
}

export bool init_smp() {
    add_idt_entry(invalidate_tlb_interrupt, (void*)invalidate_tlb_isr, false, 0, false);

    ACPI_TABLE_HEADER* tbl;
    auto status = AcpiGetTable((char*)ACPI_SIG_MADT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MADT\n", status);
        return false;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_MADT)) {
        print("MADT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_MADT));
        return false;
    }

    span madt((const uint8_t*)tbl + sizeof(ACPI_TABLE_MADT),
              tbl->Length - sizeof(ACPI_TABLE_MADT));

    uint8_t bsp_id = current_cpu_id();
    unsigned int ap_count = 0;

    auto data = madt;
    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        if (h.Type == ACPI_MADT_TYPE_LOCAL_APIC && h.Length >= sizeof(ACPI_MADT_LOCAL_APIC)) {
            auto& ent = *(ACPI_MADT_LOCAL_APIC*)data.data();

            if (ent.Id != bsp_id && ent.LapicFlags & (ACPI_MADT_ENABLED | ACPI_MADT_ONLINE_CAPABLE)) {
                uint64_t va = KERNEL_STACK | ((ent.Id * KERNEL_STACK_PAGES) << PAGE_SHIFT);

                for (unsigned int i = 0; i < KERNEL_STACK_PAGES; i++) {
                    auto pa = allocate_page();
                    if (!pa) {
                        print("allocate_page failed\n");
                        // FIXME - deallocate what's already been allocated (or BSOD)
                        return false;
                    }

                    if (!map_page(pa, va, mapping_type::read_write)) {
                        print("map_page failed\n");
                        // FIXME - deallocate what's already been allocated (or BSOD)
                        return false;
                    }

                    va += PAGE_SIZE;
                }

                ap_count++;
            }
        }

        data = data.subspan(h.Length);
    }

    apic_ids = (uint8_t*)malloc(ap_count + 1);
    if (!apic_ids) {
        print("malloc failed\n");
        return false;
    }

    apic_ids[0] = current_cpu_id();
    memset(apic_ids + 1, 0, sizeof(uint8_t) * ap_count);

    num_cpus = ap_count + 1;

    if (ap_count == 0)
        return true;

    auto trampoline = (void*)0;

    // we already reserved page 0 in the bootloader
    if (!map_page(0, (uintptr_t)trampoline, mapping_type::read_write_execute)) {
        print("map_page failed\n");
        return false;
    }

    memcpy(trampoline, &ap_trampoline, (size_t)(&ap_trampoline_end - &ap_trampoline));

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

    ap_left.store(ap_count, memory_order_acq_rel);

    data = madt;
    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        if (h.Type == ACPI_MADT_TYPE_LOCAL_APIC && h.Length >= sizeof(ACPI_MADT_LOCAL_APIC)) {
            auto& ent = *(ACPI_MADT_LOCAL_APIC*)data.data();

            if (ent.Id != bsp_id && ent.LapicFlags & (ACPI_MADT_ENABLED | ACPI_MADT_ONLINE_CAPABLE)) {
                print("CPU found\n");

                start_ap(ent.Id);
            }
        }

        data = data.subspan(h.Length);
    }

    while (ap_left.load(memory_order_acq_rel) > 0) {
        __asm("pause");
    }

    unmap_page(0);

    return true;
}
