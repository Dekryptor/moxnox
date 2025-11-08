module;

#include "timer-asm.h"
#include <string_view>
#include "acpica.h"
#include <span>
#include <atomic>

#ifdef __aarch64__
#include "aarch64/mem.h"
#else
#include "x86/mem.h"
#endif

export module smp;

#ifdef __aarch64__
import psci;
#else
import apic;
#endif

import print;
import mem;
import config;
import interrupts;
import timer;
import syscalls;
import scheduler;
import disable_interrupts;
import cpu_block;
import cmdline;

using namespace std;

#ifdef __aarch64__

// in aarch64/smp.S
extern "C" uint8_t ap_trampoline;

// in aarch64/ints.S
extern "C" uint8_t vector_table_ap;

export uint64_t* cpu_ids;

#else

// in x86/smp.S
extern "C" uint8_t ap_trampoline;
extern "C" uint8_t ap_trampoline_end;
extern "C" uint8_t ap_trampoline_cr3;
extern "C" uint64_t ap_trampoline_start_func;
extern "C" uint64_t ap_trampoline_rsp;

struct gdt_ptr {
    uint16_t limit;
    uint64_t base;
} __attribute__((packed));

static gdt_ptr bsp_gdt, bsp_idt;

export uint8_t* cpu_ids;

#endif

export unsigned int num_cpus;

static atomic<unsigned int> ap_left;

#ifndef __aarch64__
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
#endif

#ifdef __aarch64__
export uint64_t current_cpu_id() {
    uint64_t mpidr;

    __asm __volatile (
        "mrs %x[res], mpidr_el1"
        : [res] "=r" (mpidr)
    );

    return mpidr & MPIDR_MASK;
}
#else
export uint8_t current_cpu_id() {
    uint32_t eax, ebx, ecx, edx;

    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (1)
    );

    return (uint8_t)((ebx >> 24) & 0xff);
}
#endif

#ifndef __aarch64__
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
#endif

#ifdef __aarch64__
static void __attribute__((noreturn)) ap_startup() {
    disable_interrupts();

    __asm __volatile (
        "msr vbar_el1, %x[res]"
        :
        : [res] "r" (&vector_table_ap)
    );

    auto& cb = *get_cpu_block();
    auto& GICC_CTLR = *(volatile ARM64_GICC_CTLR*)cb.gicc;
    auto& GICC_PMR = *(volatile ARM64_GICC_PMR*)(cb.gicc + 0x4);

    GICC_CTLR.EnableGrp0 = 1; // enable GICC

    // allow all interrupts through
    GICC_PMR.Priority = 0xff;

    // set up timer
    __asm __volatile (
        "msr cntp_tval_el0, %x[res]"
        "\n\tmov x0, #1"
        "\n\tmsr cntp_ctl_el0, x0"
        :
        : [res] "r" (hardware_tick_rate)
        : "x0"
    );

    clear_irq_pending_bit(timer_irq_no);
    enable_irq(timer_irq_no);

    ap_left.fetch_sub(1, memory_order_acq_rel);

    acquire_sync_lock();

    __asm __volatile ( // doesn't return
        "add x0, x18, #%0\n\t"
        "ldr x0, [x0]\n\t"
        "mov sp, x0\n\t"
        "b scheduler"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );

    __builtin_unreachable();
}
#else
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

    cpu_ids[num_cpus - num] = apic_id;

    acquire_sync_lock();

    __asm __volatile ( // doesn't return
        "mov rsp, gs:%0\n"
        "jmp scheduler\n"
        :
        : "i" (offsetof(cpu_block, scheduler_stack))
    );
}
#endif

#ifdef __aarch64__
static uint64_t sctlr_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], sctlr_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

static uint64_t cpacr_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], cpacr_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

static uint64_t tcr_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], tcr_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

static uint64_t mair_el1_load() {
    uint64_t ret;

    __asm __volatile (
        "mrs %x[res], mair_el1"
        : [res] "=r" (ret)
    );

    return ret;
}

static void start_cpu(uint64_t mpidr, uint64_t entry_point_phys,
                      uint64_t boot_stack, uint64_t identity_map,
                      cpu_block* cpu_block) {
    auto boot_sp = (uint8_t*)(boot_stack + PAGE_SIZE - 0x40);
    auto boot_sp_va = (uint8_t*)phys_address((uintptr_t)boot_sp);

    *(uint64_t*)(boot_sp_va + (7 * sizeof(uint64_t))) = identity_map;
    *(uint64_t*)(boot_sp_va + (6 * sizeof(uint64_t))) = ttbr1_el1_load();
    *(uint64_t*)(boot_sp_va + (5 * sizeof(uint64_t))) = sctlr_el1_load();
    *(uint64_t*)(boot_sp_va + (4 * sizeof(uint64_t))) = cpacr_el1_load();
    *(uint64_t*)(boot_sp_va + (3 * sizeof(uint64_t))) = tcr_el1_load();
    *(uint64_t*)(boot_sp_va + (2 * sizeof(uint64_t))) = mair_el1_load();
    *(uint64_t*)(boot_sp_va + sizeof(uint64_t)) = (uintptr_t)ap_startup;
    *(uint64_t*)boot_sp_va = (uintptr_t)cpu_block;

    __asm __volatile(
        "dc cvac, %x[res]\n"
        "dsb sy\n"
        "isb\n"
        :
        : [res] "r" (boot_sp_va)
    );

    auto ret = CPU_ON(mpidr, entry_point_phys, (uint64_t)boot_sp);

    if (ret != PSCI_STATUS::SUCCESS) {
        ap_left--;
        print("CPU_ON returned {} for CPU with MPIDR {:x}\n", ret, mpidr);
        return;
    }
}

static uint64_t setup_identity_map() {
    // allocate two pages
    auto pa = allocate_folio(1);
    if (!pa) {
        print("allocate_folio failed\n");
        return 0;
    }

    auto pte0 = span((ARM64_HARDWARE_PTE*)phys_address(pa), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto pte1 = span((ARM64_HARDWARE_PTE*)phys_address(pa + PAGE_SIZE), PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));

    memset(pte0.data(), 0, pte0.size_bytes());
    memset(pte1.data(), 0, pte1.size_bytes());

    pte0[0].valid = 1;
    pte0[0].table_entry = 1;
    pte0[0].AttrIndex = 0;
    pte0[0].AP = 0; // no access from userspace
    pte0[0].SH = 3; // inner shareable
    pte0[0].AF = 1;
    pte0[0].pfn = (pa + PAGE_SIZE) >> PAGE_SHIFT;

    static const uint64_t SZ_1G = 0x40000000;

    for (unsigned int i = 0; i < pte1.size(); i++) {
        pte1[i].valid = 1;
        pte1[i].table_entry = 0;
        pte1[i].AttrIndex = 0;
        pte1[i].AP = 0; // no access from userspace
        pte1[i].SH = 3; // inner shareable
        pte1[i].AF = 1;
        pte1[i].pfn = ((uint64_t)i * SZ_1G) >> PAGE_SHIFT;
    }

    return pa;
}
#endif

export bool init_smp() {
#ifdef __aarch64__
    if (psci_version == 0) // FIXME - support spin-table
        return false;
#endif

    bool nosmp = false;
    loop_through_cmdline([&](string_view s) {
        static const string_view prefix = "console=";

        if (s == "nosmp") {
            nosmp = true;
            return false;
        }

        return true;
    });

    if (nosmp)
        return false;

    // FIXME - support devicetree

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

    unsigned int ap_count = 0;
    auto bsp_id = current_cpu_id();

    auto data = madt;
    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

#ifdef __aarch64__
        if (h.Type == ACPI_MADT_TYPE_GENERIC_INTERRUPT) {
            auto& ent = *(ACPI_MADT_GENERIC_INTERRUPT*)data.data();

            if ((ent.ArmMpidr & MPIDR_MASK) != bsp_id)
                ap_count++;
        }
#else
        if (h.Type == ACPI_MADT_TYPE_LOCAL_APIC && h.Length >= sizeof(ACPI_MADT_LOCAL_APIC)) {
            auto& ent = *(ACPI_MADT_LOCAL_APIC*)data.data();

            if (ent.Id != bsp_id && ent.LapicFlags & (ACPI_MADT_ENABLED | ACPI_MADT_ONLINE_CAPABLE))
                ap_count++;
        }
#endif

        data = data.subspan(h.Length);
    }

    cpu_ids = (decltype(bsp_id)*)malloc(ap_count + 1);
    if (!cpu_ids) {
        print("malloc failed\n");
        return false;
    }

    cpu_ids[0] = current_cpu_id();
    memset(cpu_ids + 1, 0, sizeof(decltype(bsp_id)) * ap_count);

    num_cpus = ap_count + 1;

    if (ap_count == 0)
        return true;

#ifdef __aarch64__
    {
        unsigned int i = 1;

        data = madt;
        while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
            auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

            if (h.Type == ACPI_MADT_TYPE_GENERIC_INTERRUPT) {
                auto& ent = *(ACPI_MADT_GENERIC_INTERRUPT*)data.data();

                if ((ent.ArmMpidr & MPIDR_MASK) != bsp_id) {
                    cpu_ids[i] = ent.ArmMpidr & MPIDR_MASK;
                    i++;
                }
            }

            data = data.subspan(h.Length);
        }
    }

    auto identity_map = setup_identity_map();
    if (!identity_map)
        return false;

    struct info {
        uint64_t stack;
        cpu_block* cpu_block;
    };

    auto boot_info = (info*)malloc(sizeof(info) * ap_count);
    for (unsigned int i = 0; i < ap_count; i++) {
        boot_info[i].stack = allocate_page();
        if (!boot_info[i].stack) {
            print("allocate_page failed\n");
            for (unsigned int j = 0; j < i; j++) {
                free_folio(boot_info[j].stack);
            }
            free(boot_info);
            return false;
        }

        boot_info[i].cpu_block = (cpu_block*)init_cpu_block(i + 1);

        if (!boot_info[i].cpu_block) {
            print("init_cpu_block failed\n");

            // FIXME - should free other cpu_blocks instead (or just bsod)

            for (unsigned int j = 0; j < i; j++) {
                free_folio(boot_info[j].stack);
            }
            free(boot_info);
            return false;
        }

        auto gicc = init_interrupts_cpu<false>(cpu_ids[i + 1]);
        if (!gicc) {
            print("init_interrupts_cpu failed for CPU {} (MPIDR {:x})\n",
                  i + 1, cpu_ids[i + 1]);
            return false;
        }

        boot_info[i].cpu_block->gicc = gicc;
    }

    ap_left.store(ap_count, memory_order_release);

    auto ap_trampoline_phys = va_to_pa_lookup((uint64_t)&ap_trampoline);

    for (unsigned int i = 1; i < num_cpus; i++) {
        start_cpu(cpu_ids[i], ap_trampoline_phys, boot_info[i - 1].stack,
                  identity_map, boot_info[i - 1].cpu_block);
    }

    while (ap_left.load(memory_order_acquire) > 0) {
        __asm("yield");
    }

    for (unsigned int i = 0; i < ap_count; i++) {
        free_folio(boot_info[i].stack);
    }
    free(boot_info);

    free_folio(identity_map);
#else
    struct info {
        uint8_t id;
        uint64_t stack_pa;
    };

    auto boot_info = (info*)malloc(sizeof(info) * ap_count);

    {
        unsigned int i = 0;

        auto data = madt;
        while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
            auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

            if (h.Type == ACPI_MADT_TYPE_LOCAL_APIC && h.Length >= sizeof(ACPI_MADT_LOCAL_APIC)) {
                auto& ent = *(ACPI_MADT_LOCAL_APIC*)data.data();

                if (ent.Id != bsp_id && ent.LapicFlags & (ACPI_MADT_ENABLED | ACPI_MADT_ONLINE_CAPABLE)) {
                    boot_info[i].id = ent.Id;

                    boot_info[i].stack_pa = allocate_page();
                    if (!boot_info[i].stack_pa) {
                        print("allocate_page failed\n");
                        // FIXME - deallocate what's already been allocated (or BSOD)
                        free(boot_info);
                        return false;
                    }

                    i++;
                }
            }

            data = data.subspan(h.Length);
        }
    }

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

    uint64_t* ap_rsp = (uint64_t*)((uint8_t*)trampoline + ((uint8_t*)&ap_trampoline_rsp - &ap_trampoline));

    __asm __volatile (
        "sgdt %0\n"
        "sidt %1\n"
        : "=m" (bsp_gdt), "=m" (bsp_idt)
    );

    ap_left.store(ap_count, memory_order_acq_rel);

    for (unsigned int i = 0; i < ap_count; i++) {
        if (boot_info[i].id > 63)
            print("Ignoring high-numbered CPUs (FIXME)\n"); // FIXME
        else {
            print("CPU found\n");

            *ap_rsp = (uint64_t)((uint8_t*)phys_address(boot_info[i].stack_pa) + PAGE_SIZE - 0x20); // 0x20 for MS ABI

            start_ap(boot_info[i].id);
        }
    }

    while (ap_left.load(memory_order_acq_rel) > 0) {
        __asm("pause");
    }

    unmap_page(0);

    for (unsigned int i = 0; i < ap_count; i++) {
        free_folio(boot_info[i].stack_pa);
    }

    free(boot_info);
#endif

    return true;
}
