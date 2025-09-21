module;

#include <span>
#include <atomic>
#include <string_view>
#include <optional>
#include <functional>
#include <mutex>
#include <loaderblock.h>
#include "acpica.h"
#include "cpu_block.h"
#include "mem-x86.h"
#include "spinlock.h"
#include "process.h"
#include "assert.h"
#include <span>
#include "ints.h"

export module interrupts;

#ifndef __aarch64__
import apic;
#endif

import format;
import print;
import bsod;
import mem;
import config;
import list;
import thread;
import disable_interrupts;

using namespace std;

#ifdef __aarch64__

union ARM64_ESR {
    struct {
        uint64_t ISS : 25;
        uint64_t IL : 1;
        uint64_t EC : 6;
        uint64_t ISS2 : 24;
    };
    uint64_t value;
} __attribute((packed));

union ARM64_GICD_CTLR {
    struct {
        uint32_t EnableGrp0 : 1;
        uint32_t EnableGrp1NS : 1;
        uint32_t EnableGrp1S : 1;
        uint32_t : 1;
        uint32_t ARE_S : 1;
        uint32_t ARE_NS : 1;
        uint32_t DS : 1;
        uint32_t E1NWF : 1;
        uint32_t nASSGIreq : 1;
        uint32_t : 22;
        uint32_t RWP : 1;
    };
    uint32_t value;
} __attribute((packed));

union ARM64_GICC_CTLR {
    struct {
        uint32_t EnableGrp0 : 1;
        uint32_t EnableGrp1 : 1;
        uint32_t  : 1;
        uint32_t FIQEn : 1;
        uint32_t CBPR : 1;
        uint32_t FIQBypDisGrp0 : 1;
        uint32_t IRQBypDisGrp0 : 1;
        uint32_t FIQBypDisGrp1 : 1;
        uint32_t IRQBypDisGrp1 : 1;
        uint32_t EOImodeS : 1;
        uint32_t EOImodeNS : 1;
        uint32_t : 21;
    };
    uint32_t value;
} __attribute((packed));

union ARM64_GICC_PMR {
    struct {
        uint32_t Priority : 8;
        uint32_t : 24;
    };
    uint32_t value;
} __attribute((packed));

static uint64_t gicc, gicd;

#else

// tlb.S
extern "C" void invalidate_tlb_isr();

static const uint32_t CPUID_FEAT_EDX_APIC = 0x200;
static const uint32_t IA32_APIC_BASE_MSR = 0x1b;

static const uint16_t interrupt_cs = 0x8;

typedef struct {
    uint16_t offset_1;
    uint16_t selector;
    uint8_t ist;
    uint8_t gate_type : 4;
    uint8_t zero1 : 1;
    uint8_t dpl : 2;
    uint8_t present : 1;
    uint16_t offset_2;
    uint32_t offset_3;
    uint32_t zero2;
} idt_entry;

static auto idt = span((idt_entry*)0xfffffac000000000, 256);

using isr_func_type = void (*)(void*);

static const uint8_t MIN_DYNAMIC_INTERRUPT = 0x30;

export const uint8_t halt_interrupt = 0x60;
export const uint8_t invalidate_tlb_interrupt = 0x21;
export const uint8_t spurious_interrupt = 0xff;

export uintptr_t mn_signal;

static spinlock isr_lock;

struct isr_func_info {
    isr_func_type func;
    void* ctx;
};

static isr_func_info isr_funcs[0x20];

#endif

static spinlock irq_lock;

struct irq {
    list_entry le;
    uint32_t number;
    spinlock lock;
    list_entry handlers = { &handlers, &handlers };
};

using irq_func_type = bool (*)(void*);

struct irq_handler_info {
    list_entry le;
    irq_func_type func;
    void* ctx;
};

static list_entry irqs = { &irqs, &irqs };

#ifndef __aarch64__

extern "C" void dynamic_isr_30();
extern "C" void dynamic_isr_31();
extern "C" void dynamic_isr_32();
extern "C" void dynamic_isr_33();
extern "C" void dynamic_isr_34();
extern "C" void dynamic_isr_35();
extern "C" void dynamic_isr_36();
extern "C" void dynamic_isr_37();
extern "C" void dynamic_isr_38();
extern "C" void dynamic_isr_39();
extern "C" void dynamic_isr_3a();
extern "C" void dynamic_isr_3b();
extern "C" void dynamic_isr_3c();
extern "C" void dynamic_isr_3d();
extern "C" void dynamic_isr_3e();
extern "C" void dynamic_isr_3f();
extern "C" void dynamic_isr_40();
extern "C" void dynamic_isr_41();
extern "C" void dynamic_isr_42();
extern "C" void dynamic_isr_43();
extern "C" void dynamic_isr_44();
extern "C" void dynamic_isr_45();
extern "C" void dynamic_isr_46();
extern "C" void dynamic_isr_47();
extern "C" void dynamic_isr_48();
extern "C" void dynamic_isr_49();
extern "C" void dynamic_isr_4a();
extern "C" void dynamic_isr_4b();
extern "C" void dynamic_isr_4c();
extern "C" void dynamic_isr_4d();
extern "C" void dynamic_isr_4e();
extern "C" void dynamic_isr_4f();

static bool check_cpu_for_apic() {
    uint32_t eax, ebx, ecx, edx;

    __asm__ __volatile__ (
        "cpuid\n\t"
        : "=a" (eax), "=b" (ebx), "=c" (ecx), "=d" (edx)
        : "0" (1)
    );

    if (!(edx & CPUID_FEAT_EDX_APIC)) {
        print("APIC disabled or not supported by CPU.\n");
        return false;
    }

    return true;
}

static inline uint64_t rdmsr(uint64_t msr) {
    uint32_t low, high;

    __asm __volatile (
        "rdmsr"
        : "=a" (low), "=d" (high)
        : "c"(msr)
    );

    return ((uint64_t)high << 32) | low;
}

static uint32_t get_irq_gsi(span<const uint8_t> madt, uint32_t irq) {
    auto data = madt;

    // check source overrides in MADT

    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        if (h.Type == ACPI_MADT_TYPE_INTERRUPT_OVERRIDE && h.Length >= sizeof(ACPI_MADT_INTERRUPT_OVERRIDE)) {
            auto& ent = *(ACPI_MADT_INTERRUPT_OVERRIDE*)data.data();

            if (ent.Bus == 0 && ent.SourceIrq == irq)
                return ent.GlobalIrq;

            break;
        }

        data = data.subspan(h.Length);
    }

    return irq;
}

static ACPI_MADT_IO_APIC* find_ioapic(span<const uint8_t> madt, uint32_t gsi) {
    auto data = madt;

    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        if (h.Type == ACPI_MADT_TYPE_IO_APIC && h.Length >= sizeof(ACPI_MADT_IO_APIC)) {
            auto& ent = *(ACPI_MADT_IO_APIC*)data.data();

            if (gsi >= ent.GlobalIrqBase) {
                volatile uint32_t* IOREGSEL = (uint32_t*)phys_address(ent.Address);
                volatile uint32_t* IOREGWIN = (uint32_t*)phys_address(ent.Address + 0x10);

                auto read_reg = [&](uint32_t reg) -> uint32_t {
                    *IOREGSEL = reg;

                    return *IOREGWIN;
                };

                auto IOAPICVER = read_reg(1);
                auto max_entry = (IOAPICVER & 0xff0000) >> 16;

                if (gsi <= ent.GlobalIrqBase + max_entry)
                    return &ent;
            }
        }

        data = data.subspan(h.Length);
    }

    return nullptr;
}

export uint8_t add_isr(isr_func_type isr_func, void* ctx) {
    lock_guard lock(isr_lock);

    auto sp = span(isr_funcs);

    for (uint8_t i = 0; i < sp.size(); i++) {
        if (!sp[i].func) {
            sp[i].func = isr_func;
            sp[i].ctx = ctx;
            return MIN_DYNAMIC_INTERRUPT + i;
        }
    }

    return 0;
}

// FIXME - we could remove the search if we add the int_num as a parameter instead
export void remove_isr(isr_func_type isr_func, void* ctx) {
    lock_guard lock(isr_lock);

    auto sp = span(isr_funcs);

    for (uint8_t i = 0; i < sp.size(); i++) {
        if (sp[i].func == isr_func && sp[i].ctx == ctx) {
            sp[i].func = nullptr;
            break;
        }
    }
}

static void irq_handler(void* ctx) {
    auto& i = *(irq*)ctx;

    {
        lock_guard lock(i.lock);

        for (auto le = i.handlers.next; le != &i.handlers; le = le->next) {
            auto& h = containing_record<irq_handler_info>(le);

            if (h.func(h.ctx))
                return;
        }
    }
}

static irq* find_irq(uint32_t number, uint32_t apic_id) {
    {
        lock_guard lock(irq_lock);

        for (auto le = irqs.next; le != &irqs; le = le->next) {
            auto& i = containing_record<irq>(le);

            if (i.number == number)
                return &i;
        }
    }

    ACPI_TABLE_HEADER* tbl;
    auto status = AcpiGetTable((char*)ACPI_SIG_MADT, 1, &tbl);
    if (ACPI_FAILURE(status)) {
        print("AcpiGetTable returned {} for MADT\n", status);
        return nullptr;
    }

    if (tbl->Length < sizeof(ACPI_TABLE_MADT)) {
        print("MADT table was {:x} bytes, expected at least {:x}\n",
              tbl->Length, sizeof(ACPI_TABLE_MADT));
        return nullptr;
    }

    span madt((const uint8_t*)tbl + sizeof(ACPI_TABLE_MADT),
              tbl->Length - sizeof(ACPI_TABLE_MADT));

    auto gsi = get_irq_gsi(madt, number);
    ACPI_MADT_IO_APIC* ioapic = nullptr;
    uint8_t vec = 0;

    auto i = new irq;

    i->number = number;

    do {
        lock_guard lock(irq_lock);

        vec = add_isr(irq_handler, i);
        if (vec == 0)
            break;

        ioapic = find_ioapic(madt, gsi);

        if (!ioapic)
            break;

        auto ioapic_address = phys_address(ioapic->Address);

        volatile uint32_t* IOREGSEL = (uint32_t*)ioapic_address;
        volatile uint32_t* IOREGWIN = (uint32_t*)((uint8_t*)ioapic_address + 0x10);

        auto write_reg = [&](uint32_t reg, uint32_t val) {
            *IOREGSEL = reg;
            *IOREGWIN = val;
        };

        write_reg(0x10 + ((gsi - ioapic->GlobalIrqBase) * 2), vec);
        write_reg(0x11 + ((gsi - ioapic->GlobalIrqBase) * 2), apic_id);

        insert_tail_list(irqs, i->le);
    } while (false);

    if (vec == 0) {
        print("add_isr failed\n");
        return nullptr;
    }

    if (!ioapic) { // print outside of lock
        print("Could not find I/O APIC for GSI {:x}.\n", gsi);
        // FIXME - free interrupt number
        return nullptr;
    }

    return i;
}

#endif

#ifdef __aarch64__

static irq* find_irq(uint32_t number) {
    {
        lock_guard lock(irq_lock);

        for (auto le = irqs.next; le != &irqs; le = le->next) {
            auto& i = containing_record<irq>(le);

            if (i.number == number)
                return &i;
        }
    }

    auto i = new irq;

    i->number = number;

    {
        lock_guard lock(irq_lock);

        for (auto le = irqs.next; le != &irqs; le = le->next) {
            auto& i2 = containing_record<irq>(le);

            if (i2.number == number) {
                delete i;
                return &i2;
            }
        }

        insert_tail_list(irqs, i->le);

        // clear pending bit if new IRQ
        auto& GICD_ICPENDR = *(volatile uint32_t*)(gicd + 0x200 + ((number / 32) * 4));
        GICD_ICPENDR = 1 << (number % 32);
    }

    return i;
}

export bool add_irq_handler(uint32_t number, irq_func_type func, void* ctx) {
    auto irq = find_irq(number);
    if (!irq)
        return false;

    auto info = new irq_handler_info;
    if (!info)
        return false;

    info->func = func;
    info->ctx = ctx;

    {
        lock_guard lock(irq->lock);
        insert_tail_list(irq->handlers, info->le);
    }

    // enable IRQ
    auto& GICD_ISENABLER = *(volatile uint32_t*)(gicd + 0x100 + ((number / 32) * 4));
    GICD_ISENABLER = 1 << (number % 32);

    return true;
}

#else

export bool add_irq_handler(uint32_t number, uint32_t apic_id,
                            irq_func_type func, void* ctx) {
    auto irq = find_irq(number, apic_id);
    if (!irq)
        return false;

    auto info = new irq_handler_info;
    if (!info)
        return false;

    info->func = func;
    info->ctx = ctx;

    {
        lock_guard lock(irq->lock);
        insert_tail_list(irq->handlers, info->le);
    }

    return true;
};

#endif

#ifndef __aarch64__

export bool init_apic() {
    // FIXME - can we assume that nothing's disabled the APIC? (Check EFI specs?)

    if (!check_cpu_for_apic())
        return false;

    // disable 8259 PIC
    __asm __volatile (
        "mov al, 0xff\n"
        "out 0xa1, al\n"
        "out 0x21, al\n"
        :
        :
        : "al"
    );

    auto apic_base = rdmsr(IA32_APIC_BASE_MSR) & 0xfffff000;

    if (!map_page(apic_base, (uintptr_t)local_apic, mapping_type::no_cache)) {
        print("map_page failed for local APIC\n");
        return false;
    }

    auto spurious_interrupt_reg = (volatile uint32_t*)&local_apic[0xf0];

    *spurious_interrupt_reg = spurious_interrupt | 0x100;

    return true;
}

export void add_idt_entry(uint8_t vector, void* func, bool trap, uint8_t ist,
                          bool user_mode) {
    auto& i = idt[vector];
    auto addr = (uintptr_t)func;

    i.offset_1 = addr & 0xffff;
    i.selector = interrupt_cs;
    i.ist = ist;
    i.present = 1;
    i.dpl = user_mode ? 3 : 0;
    i.zero1 = 0;

    if (trap)
        i.gate_type = 0xf; // trap gate
    else
        i.gate_type = 0xe; // interrupt gate

    i.offset_2 = (addr >> 16) & 0xffff;
    i.offset_3 = (uint32_t)(addr >> 32);
    i.zero2 = 0;
}

extern "C"
void handle_dynamic_isr(uint8_t int_num) {
    isr_func_type func;
    void* ctx;

    if (int_num < MIN_DYNAMIC_INTERRUPT)
        return;

    if ((uint8_t)(int_num - MIN_DYNAMIC_INTERRUPT) > sizeof(isr_funcs) / sizeof(isr_funcs[0]))
        return;

    {
        lock_guard lock(isr_lock);
        func = isr_funcs[int_num - MIN_DYNAMIC_INTERRUPT].func;
        ctx = isr_funcs[int_num - MIN_DYNAMIC_INTERRUPT].ctx;
    }

    if (!func)
        return;

    func(ctx);
}

static bool setup_idt() {
    auto idt_page = allocate_page();
    if (idt_page == 0) {
        print("Could not allocate page for IDT.\n");
        return false;
    }

    if (!map_page(idt_page, (uintptr_t)idt.data(), mapping_type::read_write)) {
        print("map_page failed for IDT\n");
        return false;
    }

    memset(idt.data(), 0, PAGE_SIZE);

    add_idt_entry(0x0, (void*)divide_by_zero_isr, false, 0, false);
    add_idt_entry(0x1, (void*)single_step_isr, false, 0, false);
    // FIXME - int 2 (NMI)
    add_idt_entry(0x3, (void*)breakpoint_isr, false, 0, true);
    // FIXME - int 4 (overflow)
    // FIXME - int 5 (bound range exceeded)
    add_idt_entry(0x6, (void*)invalid_opcode_isr, false, 0, false);
    // FIXME - int 7 (device not available)
    add_idt_entry(0x8, (void*)double_fault_isr, false, 0, false);
    // FIXME - int 9 (coprocessor segment overrun)
    // FIXME - int a (invalid TSS)
    // FIXME - int b (segment not present)
    // FIXME - int c (stack-segment fault)
    add_idt_entry(0xd, (void*)general_protection_fault_isr, false, 0, false);
    add_idt_entry(0xe, (void*)page_fault_isr, false, 0, false);
    // FIXME - int 10 (x87 exception)
    // FIXME - int 11 (alignment check)
    // FIXME - int 12 (machine check)
    // FIXME - int 13 (SIMD exception)
    // FIXME - int 14 (virtualization exception)
    // FIXME - int 15 (control protection exception)

    add_idt_entry(0x30, (void*)dynamic_isr_30, false, 0, false);
    add_idt_entry(0x31, (void*)dynamic_isr_31, false, 0, false);
    add_idt_entry(0x32, (void*)dynamic_isr_32, false, 0, false);
    add_idt_entry(0x33, (void*)dynamic_isr_33, false, 0, false);
    add_idt_entry(0x34, (void*)dynamic_isr_34, false, 0, false);
    add_idt_entry(0x35, (void*)dynamic_isr_35, false, 0, false);
    add_idt_entry(0x36, (void*)dynamic_isr_36, false, 0, false);
    add_idt_entry(0x37, (void*)dynamic_isr_37, false, 0, false);
    add_idt_entry(0x38, (void*)dynamic_isr_38, false, 0, false);
    add_idt_entry(0x39, (void*)dynamic_isr_39, false, 0, false);
    add_idt_entry(0x3a, (void*)dynamic_isr_3a, false, 0, false);
    add_idt_entry(0x3b, (void*)dynamic_isr_3b, false, 0, false);
    add_idt_entry(0x3c, (void*)dynamic_isr_3c, false, 0, false);
    add_idt_entry(0x3d, (void*)dynamic_isr_3d, false, 0, false);
    add_idt_entry(0x3e, (void*)dynamic_isr_3e, false, 0, false);
    add_idt_entry(0x3f, (void*)dynamic_isr_3f, false, 0, false);
    add_idt_entry(0x40, (void*)dynamic_isr_40, false, 0, false);
    add_idt_entry(0x41, (void*)dynamic_isr_41, false, 0, false);
    add_idt_entry(0x42, (void*)dynamic_isr_42, false, 0, false);
    add_idt_entry(0x43, (void*)dynamic_isr_43, false, 0, false);
    add_idt_entry(0x44, (void*)dynamic_isr_44, false, 0, false);
    add_idt_entry(0x45, (void*)dynamic_isr_45, false, 0, false);
    add_idt_entry(0x46, (void*)dynamic_isr_46, false, 0, false);
    add_idt_entry(0x47, (void*)dynamic_isr_47, false, 0, false);
    add_idt_entry(0x48, (void*)dynamic_isr_48, false, 0, false);
    add_idt_entry(0x49, (void*)dynamic_isr_49, false, 0, false);
    add_idt_entry(0x4a, (void*)dynamic_isr_4a, false, 0, false);
    add_idt_entry(0x4b, (void*)dynamic_isr_4b, false, 0, false);
    add_idt_entry(0x4c, (void*)dynamic_isr_4c, false, 0, false);
    add_idt_entry(0x4d, (void*)dynamic_isr_4d, false, 0, false);
    add_idt_entry(0x4e, (void*)dynamic_isr_4e, false, 0, false);
    add_idt_entry(0x4f, (void*)dynamic_isr_4f, false, 0, false);

    add_idt_entry(halt_interrupt, (void*)halt_isr, false, 0, false);
    add_idt_entry(spurious_interrupt, (void*)spurious_isr, false, 0, false);
    add_idt_entry(yield_interrupt, (void*)yield_isr, false, 0, false);
    add_idt_entry(invalidate_tlb_interrupt, (void*)invalidate_tlb_isr, false,
                  0, false);

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) idt_ptr;

    idt_ptr.base = (uintptr_t)idt.data();
    idt_ptr.limit = (uint16_t)(idt.size_bytes() - 1);

    __asm__ __volatile__ (
        "lidt %0"
        :
        : "m" (idt_ptr)
    );

    return true;
}

export task_state_segment* init_tss() {
    auto pa = allocate_page();
    if (!pa) {
        print("allocate_page failed\n");
        return nullptr;
    }

    auto& tss = *(task_state_segment*)phys_address(pa);
    memset(&tss, 0, sizeof(tss));

    auto stack = allocate_stack();
    if (!stack) {
        free_folio(pa);
        return nullptr;
    }

    tss.ist1 = (uintptr_t)((uint8_t*)stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT));

    stack = allocate_stack();
    if (!stack) {
        free_folio(pa);
        return nullptr;
    }

    tss.rsp0 = (uintptr_t)((uint8_t*)stack + ((KERNEL_STACK_PAGES + KERNEL_GUARD_PAGES) << PAGE_SHIFT));

    return &tss;
}

static bool setup_gdt(task_state_segment* tss) {
    auto& cb = *get_cpu_block();
    uint64_t pa;

    struct {
        uint16_t limit;
        uint64_t base;
    } __attribute__((packed)) gdt_ptr;

    pa = allocate_page();
    if (pa == 0)
        return false;

    cb.gdt = phys_address(pa);

    auto gdtarr = span((gdt_entry*)cb.gdt, 8);

    *(uint64_t*)&gdtarr[0] = 0;

    // code
    gdtarr[1].LimitLow = 0xffff;
    gdtarr[1].BaseLow = 0x0000;
    gdtarr[1].BaseMid = 0x00;
    gdtarr[1].Type = 0x1a;
    gdtarr[1].Dpl = 0;
    gdtarr[1].Pres = 1;
    gdtarr[1].LimitHi = 0xf;
    gdtarr[1].Sys = 0;
    gdtarr[1].Long = 1;
    gdtarr[1].Default_Big = 0;
    gdtarr[1].Granularity = 1;
    gdtarr[1].BaseHi = 0x00;

    // data
    gdtarr[2].LimitLow = 0xffff;
    gdtarr[2].BaseLow = 0x0000;
    gdtarr[2].BaseMid = 0x00;
    gdtarr[2].Type = 0x12;
    gdtarr[2].Dpl = 0;
    gdtarr[2].Pres = 1;
    gdtarr[2].LimitHi = 0xf;
    gdtarr[2].Sys = 0;
    gdtarr[2].Long = 1;
    gdtarr[2].Default_Big = 0;
    gdtarr[2].Granularity = 1;
    gdtarr[2].BaseHi = 0x00;

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

    // FIXME - gdtarr[5] would be 32-bit user-mode code

    // user-mode data
    gdtarr[6].LimitLow = 0xffff;
    gdtarr[6].BaseLow = 0x0000;
    gdtarr[6].BaseMid = 0x00;
    gdtarr[6].Type = 0x12;
    gdtarr[6].Dpl = 3;
    gdtarr[6].Pres = 1;
    gdtarr[6].LimitHi = 0xf;
    gdtarr[6].Sys = 0;
    gdtarr[6].Long = 1;
    gdtarr[6].Default_Big = 0;
    gdtarr[6].Granularity = 1;
    gdtarr[6].BaseHi = 0x00;

    // user-mode code
    gdtarr[7].LimitLow = 0xffff;
    gdtarr[7].BaseLow = 0x0000;
    gdtarr[7].BaseMid = 0x00;
    gdtarr[7].Type = 0x1a;
    gdtarr[7].Dpl = 3;
    gdtarr[7].Pres = 1;
    gdtarr[7].LimitHi = 0xf;
    gdtarr[7].Sys = 0;
    gdtarr[7].Long = 1;
    gdtarr[7].Default_Big = 0;
    gdtarr[7].Granularity = 1;
    gdtarr[7].BaseHi = 0x00;

    gdt_ptr.limit = (uint16_t)(gdtarr.size_bytes() - 1);
    gdt_ptr.base = (uintptr_t)gdtarr.data();

    __asm__ __volatile__ (
        "lgdt %0\n"
        "push 0x8\n"
        "lea rax, [rip+label]\n"
        "push rax\n"
        "retfq\n"
        "label:\n"
        :
        : "m" (gdt_ptr)
        : "rax"
    );

    __asm__ __volatile__ (
        "mov ax, %0\n"
        "ltr ax\n"
        :
        : "i" (TSS_SEGMENT)
        : "ax"
    );

    return true;
}
#endif

#ifdef __aarch64__
static bool find_gic_addresses(optional<uint64_t>& gicc_addr,
                               optional<uint64_t>& gicd_addr) {
    ACPI_TABLE_HEADER* tbl;

    // FIXME - devicetree version

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

    auto data = madt;
    while (data.size() > sizeof(ACPI_SUBTABLE_HEADER)) {
        auto& h = *(ACPI_SUBTABLE_HEADER*)data.data();

        switch (h.Type) {
            case ACPI_MADT_TYPE_GENERIC_INTERRUPT:
                if (!gicc_addr.has_value()) {
                    auto& ent = *(ACPI_MADT_GENERIC_INTERRUPT*)data.data();

                    gicc_addr = ent.BaseAddress;
                }
                break;

            case ACPI_MADT_TYPE_GENERIC_DISTRIBUTOR:
                if (!gicd_addr.has_value()) {
                    auto& ent = *(ACPI_MADT_GENERIC_DISTRIBUTOR*)data.data();

                    gicd_addr = ent.BaseAddress;
                }
                break;
        }

        data = data.subspan(h.Length);
    }

    return true;
}

export bool init_interrupts() {
    optional<uint64_t> gicc_addr, gicd_addr;

    load_vector_table();

    if (!find_gic_addresses(gicc_addr, gicd_addr))
        return false;

    if (!gicc_addr.has_value()) {
        print("unable to find address of GIC controller\n");
        return false;
    }

    if (!gicd_addr.has_value()) {
        print("unable to find address of GIC distributor\n");
        return false;
    }

    auto va = allocate_va(1);
    if (!va) {
        print("allocate_va failed\n");
        return false;
    }

    if (!map_page(*gicc_addr, va, mapping_type::no_cache)) {
        print("map_page failed\n");
        free_va(va, 1);
        return false;
    }

    gicc = va;

    va = allocate_va(1);
    if (!va) {
        print("allocate_va failed\n");
        return false;
    }

    if (!map_page(*gicd_addr, va, mapping_type::no_cache)) {
        print("map_page failed\n");
        free_va(va, 1);
        return false;
    }

    gicd = va;

    auto& GICD_CTLR = *(volatile ARM64_GICD_CTLR*)gicd;
    auto& GICC_CTLR = *(volatile ARM64_GICC_CTLR*)gicc;
    auto& GICC_PMR = *(volatile ARM64_GICC_PMR*)(gicc + 0x4);

    GICD_CTLR.EnableGrp0 = 1; // enable GICD
    GICC_CTLR.EnableGrp0 = 1; // enable GICC

    // allow all interrupts through
    GICC_PMR.Priority = 0xff;

    return true;
}
#else
export bool init_interrupts() {
    if (!setup_idt())
        return false;

    auto tss = init_tss();
    if (!tss)
        return false;

    if (!setup_gdt(tss))
        return false;

    return true;
}
#endif

#ifndef __aarch64__
static bool send_signal(const EXCEPTION_RECORD& er, const CONTEXT& ctx,
                        nt_status exit_status) {
    auto& current_thread = *get_current_thread();
    auto& p = *current_thread.proc;

    uintptr_t us_context = ctx.Rsp - sizeof(CONTEXT);

    us_context &= ~(uint64_t)(alignof(CONTEXT) - 1);

    assert(!(us_context & (alignof(CONTEXT) - 1)));

    if (!p.copy_to((void*)us_context, &ctx, sizeof(CONTEXT))) {
        print("Unable to copy thread context to stack.\n");
        current_thread.exit_status = exit_status;
        terminate_thread(current_thread, true);
        return false;
    }

    size_t er_size = offsetof(EXCEPTION_RECORD, ExceptionInformation) + (sizeof(uintptr_t) * er.NumberParameters);
    uintptr_t us_er = us_context - er_size;

    us_er &= ~(uint64_t)(alignof(EXCEPTION_RECORD) - 1);

    assert(!(us_er & (alignof(EXCEPTION_RECORD) - 1)));

    if (!p.copy_to((void*)us_er, &er, er_size)) {
        print("Unable to copy exception record to stack.\n");
        current_thread.exit_status = exit_status;
        terminate_thread(current_thread, true);
        return false;
    }

    disable_interrupts();

    auto& cb = *get_cpu_block();

    memcpy(&cb.thread_context, &ctx, sizeof(CONTEXT));
    cb.thread_context.Rip = p.ntdll_addr + mn_signal;
    cb.thread_context.Rcx = us_er;
    cb.thread_context.Rdx = us_context;
    cb.thread_context.Rsp = us_er;

    if (exit_status == nt_status::SINGLE_STEP) {
        // clear TF flag when we return
        cb.thread_context.EFlags &= ~(uint32_t)0x100;
    }

    return true;
}

extern "C"
bool page_fault_user(uintptr_t address, uint64_t error_code) {
    auto& current_thread = *get_current_thread();
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));

    enable_interrupts();

    auto& p = *current_thread.proc;

    auto okay = p.handle_page_fault(address, error_code);

    if (okay) {
        disable_interrupts();
        memcpy(&get_cpu_block()->thread_context, &ctx, sizeof(CONTEXT));
        return false;
    }

    EXCEPTION_RECORD er;

    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.ExceptionCode = (uint32_t)nt_status::ACCESS_VIOLATION;

    er.NumberParameters = 2;
    er.ExceptionInformation[0] = (error_code & 0x12) >> 1;
    er.ExceptionInformation[1] = address;

    return send_signal(er, ctx, nt_status::ACCESS_VIOLATION);
}

extern "C"
void page_fault_kernel(uintptr_t address, uint64_t error_code) {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Page fault in kernel mode (address = {:x}, error code = {:x}, ", address, error_code);

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {})\n", cb.cpu_no);

    if (address & ((uint64_t)1 << 63)) {
        uint64_t pte = SELFMAP | ((address & 0xfffffffff000) >> 9);

        sp = format(sp, "pte {:x}\n", pte);
    }

    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)));
}

extern "C"
void double_fault() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Double fault, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)));
}

extern "C"
void general_protection_fault_kernel(uint64_t error_code) {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "General protection fault in kernel mode, error code = {:x}, ", error_code);

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)));
}

extern "C"
void general_protection_fault_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::ACCESS_VIOLATION; // sic
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 2;
    er.ExceptionInformation[0] = 0;
    er.ExceptionInformation[1] = 0xffffffffffffffff;

    send_signal(er, ctx, nt_status::ACCESS_VIOLATION);
}

#endif

extern "C"
void invalid_opcode_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Invalid opcode in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)));
}

#ifndef __aarch64__

extern "C"
void invalid_opcode_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::ILLEGAL_INSTRUCTION;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::ILLEGAL_INSTRUCTION);
}

extern "C"
void divide_by_zero_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Divide by zero in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)));
}

extern "C"
void divide_by_zero_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::INTEGER_DIVIDE_BY_ZERO;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::INTEGER_DIVIDE_BY_ZERO);
}

extern "C"
void breakpoint_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Unhandled breakpoint in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)));
}

extern "C"
void breakpoint_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::BREAKPOINT;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::BREAKPOINT);
}

extern "C"
void single_step_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Unhandled single-step breakpoint in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)));
}

extern "C"
void single_step_user() {
    CONTEXT ctx;

    memcpy(&ctx, &get_cpu_block()->thread_context, sizeof(CONTEXT));

    enable_interrupts();

    EXCEPTION_RECORD er;

    er.ExceptionCode = (uint32_t)nt_status::SINGLE_STEP;
    er.ExceptionFlags = 0;
    er.ExceptionRecord = nullptr;
    er.ExceptionAddress = (void*)ctx.Rip;
    er.NumberParameters = 0;

    send_signal(er, ctx, nt_status::SINGLE_STEP);
}

#endif

#ifdef __aarch64__

static void unhandled_kernel_exception(uint64_t esr_value) {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;
    ARM64_ESR esr;

    esr.value = esr_value;

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Unhandled exception in kernel mode, ");

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {}\n", cb.cpu_no);
    sp = format(sp, "ESR_EL1 = {:08x} (ISS = {:x}, IL = {:x}, EC = {:x}, ISS2 = {:x})\n",
                esr_value, esr.ISS, esr.IL, esr.EC, esr.ISS2);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)));
}

static void page_fault_kernel() {
    auto& cb = *get_cpu_block();
    auto current_thread = cb.current_thread;
    uint64_t address;

    __asm __volatile (
        "mrs %x[res], far_el1"
        : [res] "=r" (address)
    );

    // FIXME - print what variety of page fault it is, using ISS in ESR_EL1

    char s[1024];
    auto sp = span(s, sizeof(s));

    sp = format(sp, "Page fault in kernel mode (address = {:x}, ", address);

    if (!current_thread)
        sp = format(sp, "not in thread");
    else
        sp = format(sp, "thread {:x}", (uintptr_t)current_thread);

    sp = format(sp, ", CPU {})\n", cb.cpu_no);
    sp = format(sp, "{}", cb.thread_context);

    bsod(string_view(s, (size_t)(sp.data() - s)));
}

extern "C"
void handle_kernel_exception(uint64_t esr_value) {
    ARM64_ESR esr;

    esr.value = esr_value;

    switch (esr.EC) {
        case 0: // "Unknown reason"
            // FIXME - might actually be other things
            invalid_opcode_kernel();
            break;

        case 0x21: // Instruction Abort from same EL
        case 0x25: // Data Abort from same EL
            page_fault_kernel();
            break;

        default:
            unhandled_kernel_exception(esr_value);
            break;
    }
}

#endif
