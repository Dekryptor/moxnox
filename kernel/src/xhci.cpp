module;

#include "kernel.h"
#include "xhci.h"
#include "spinlock.h"

export module xhci;

import pci;
import print;
import mem;
import config;
import interrupts;
import event;

using namespace std;

static const uint16_t EVENT_RING_SIZE = 64;
static const uint16_t COMMAND_RING_SIZE = 64;

struct xhci_request {
    xhci_trb_common trb;
    xhci_trb_command_completion comp;
    object_ref event;
};

struct port_status_change {
    list_entry le;
    uint8_t port_id;
};

struct xhci_port {
    uint8_t slot;
};

class xhci {
public:
    xhci(pci_device& dev);
    ~xhci();
    void init_ports();
    void submit(xhci_request& req);
    void isr();
    void port_status_change_thread();

    bool initialized = false;

private:
    void isr_command_completion(xhci_trb_command_completion& comp);
    void isr_port_status_change(xhci_trb_port_status_change& psc);
    void port_status_changed(uint8_t port_id);
    uint8_t enable_slot();
    bool address_device(uint8_t slot, uint8_t port_id);

    pci_device& dev;
    uint8_t num_slots, num_ports;
    uint64_t* dcbaa = nullptr;
    xhci_trb_common* command_ring = nullptr;
    xhci_trb_common* event_ring = nullptr;
    unsigned int event_ring_offset = 0;
    xhci_event_ring_segment* seg_table = nullptr;
    uint8_t caplength;
    uint32_t dboff;
    uint32_t rtsoff;
    xhci_request* reqs[COMMAND_RING_SIZE];
    spinlock submit_lock;
    unsigned int command_ring_index = 0;
    bool cycle_bit = true;
    object_ref port_status_change_event;
    object_ref port_status_change_thread_obj;
    spinlock port_status_change_lock;
    list_entry port_status_change_list = { &port_status_change_list, &port_status_change_list };
    xhci_port* ports = nullptr;
};

static void write_volatile64(volatile uint64_t* addr, uint64_t val) {
    *(volatile uint32_t*)addr = val & 0xffffffff;
    *((volatile uint32_t*)addr + 1) = (uint32_t)(val >> 32);
}

static void xhci_isr_func(void* ctx) {
    ((xhci*)ctx)->isr();
}

xhci::xhci(pci_device& dev) : dev(dev) {
    dev.header->command |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY;

    if (!dev.msix && dev.msi.empty()) {
        print("init_xhci: no MSI or MSI-X capability\n");
        return;
    }

    if (dev.bar[0].size() < sizeof(xhci_cap_regs)) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), sizeof(xhci_cap_regs));
        return;
    }

    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();

    if (cap_regs.CAPLENGTH < 0x20) {
        print("init_xhci: CAPLENGTH was {}, expected at least {}\n",
              cap_regs.CAPLENGTH, 0x20);
        return;
    }

    auto hcsparams1 = cap_regs.HCSPARAMS1;

    num_slots = hcsparams1 & 0xff;
    num_ports = (uint8_t)(hcsparams1 >> 24);

    if (num_ports == 0) {
        print("xhci::xhci: num_ports was 0\n");
        return;
    }

    ports = (xhci_port*)malloc(sizeof(xhci_port) * num_ports);
    if (!ports) {
        print("malloc failed\n");
        return;
    }

    memset(ports, 0, sizeof(xhci_port) * num_ports);

    if (dev.bar[0].size() < cap_regs.CAPLENGTH + offsetof(xhci_op_regs, ports) + (num_ports * sizeof(xhci_port_regs))) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), cap_regs.CAPLENGTH + sizeof(xhci_op_regs) + (num_ports * sizeof(xhci_port_regs)));
        return;
    }

    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    op_regs.USBCMD &= ~XHCI_USBCMD_RS; // unset run bit
    for (unsigned int i = 0; i < 16; i++) { // specs say we should wait up to 16ms
        if (op_regs.USBSTS & 1) // halted
            break;

        pit_delay(PIT_HZ / 1000); // wait 1ms
    }

    op_regs.USBCMD = XHCI_USBCMD_HCRST; // reset

    bool reset_done = false;

    // wait for reset to complete
    for (unsigned int i = 0; i < 500; i++) {
        if (!(op_regs.USBCMD & XHCI_USBCMD_HCRST)) {
            reset_done = true;
            break;
        }

        pit_delay(PIT_HZ / 1000); // wait 1ms
    }

    if (!reset_done) {
        print("init_xhci: timed out resetting controller\n");
        return;
    }

    print("init_xhci: num_slots = {}, num_ports = {}\n", num_slots, num_ports);

    dcbaa = (uint64_t*)malloc((num_slots + 1) * sizeof(uint64_t));
    if (!dcbaa) {
        print("malloc failed\n");
        return;
    }

    memset(dcbaa, 0, (num_slots + 1) * sizeof(uint64_t));

    write_volatile64(&op_regs.DCBAAP, va_to_pa(dcbaa));

    command_ring = (xhci_trb_common*)malloc((COMMAND_RING_SIZE + 1) * sizeof(xhci_trb_common));
    if (!command_ring) {
        print("malloc failed\n");
        return;
    }

    memset(command_ring, 0, (COMMAND_RING_SIZE + 1) * sizeof(xhci_trb_common));

    auto& cmd_link = *reinterpret_cast<xhci_trb_link*>(&command_ring[COMMAND_RING_SIZE]);
    cmd_link.ring_segment_pointer = va_to_pa(command_ring);
    cmd_link.toggle_cycle = 1;
    cmd_link.trb_type = (uint16_t)xhci_trb_type::link;

    write_volatile64(&op_regs.CRCR, va_to_pa(command_ring) | 1);

    op_regs.CONFIG = (op_regs.CONFIG & ~(uint32_t)0xff) | num_slots;

    if (dev.bar[0].size() < cap_regs.RTSOFF + offsetof(xhci_run_regs, int_regs) + sizeof(xhci_int_regs)) {
        print("init_xhci: BAR0 was {} bytes, expected at least {}\n",
              dev.bar[0].size(), cap_regs.RTSOFF + offsetof(xhci_run_regs, int_regs) + sizeof(xhci_int_regs));
        return;
    }

    volatile auto& run_regs = *(xhci_run_regs*)((uint8_t*)&cap_regs + cap_regs.RTSOFF);

    event_ring = (xhci_trb_common*)malloc(EVENT_RING_SIZE * sizeof(xhci_trb_common));
    if (!event_ring) {
        print("malloc failed\n");
        return;
    }

    memset(event_ring, 0, EVENT_RING_SIZE * sizeof(xhci_trb_common));

    seg_table = (xhci_event_ring_segment*)malloc_aligned(sizeof(xhci_event_ring_segment), 64);
    if (!seg_table) {
        print("malloc failed\n");
        return;
    }

    memset(seg_table, 0, sizeof(xhci_event_ring_segment));

    seg_table->base_address = va_to_pa(event_ring);
    seg_table->size = EVENT_RING_SIZE;

    volatile auto& int_reg = run_regs.int_regs[0];

    int_reg.ERSTSZ = 1;
    write_volatile64(&int_reg.ERDP, va_to_pa(event_ring));
    write_volatile64(&int_reg.ERSTBA, va_to_pa(seg_table));

    port_status_change_event = create_event(true);

    // FIXME - stop thread if xhci object goes away(?)
    port_status_change_thread_obj = add_kernel_thread([](void* ctx) {
        ((xhci*)ctx)->port_status_change_thread();
    }, this);

    // FIXME - free interrupt number in destructor
    auto int_num = add_isr(xhci_isr_func, this);
    if (int_num == 0) {
        print("add_isr failed\n");
        return;
    }

    if (dev.msix) {
        // enable MSI-X
        dev.msix->message_control |= 0x8000;

        // set up interrupt
        auto& vec = *(volatile msix_entry*)(dev.bar[dev.msix->bir].data() + (dev.msix->table_offset << 3));

        vec.message_data = int_num;
        vec.message_address_high = 0;
        vec.message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors
        vec.vector_control &= ~(uint32_t)1; // unmask
    } else {
        auto& msi = *(volatile pci_msi*)dev.msi.data();

        msi.message_address_low = apic_msi_address(0, false, false); // FIXME - spread out amongst processors

        if (msi.message_control & 0x80) { // 64-bit
            msi.v64.message_address_high = 0;
            msi.v64.message_data = int_num;
        } else
            msi.v32.message_data = int_num;

        msi.message_control = msi.message_control | 1; // enable
    }

    int_reg.IMAN |= XHCI_IMAN_IE;

    op_regs.USBCMD |= XHCI_USBCMD_INTE | XHCI_USBCMD_RS;

    caplength = cap_regs.CAPLENGTH;
    dboff = cap_regs.DBOFF;
    rtsoff = cap_regs.RTSOFF;

    initialized = true;
}

xhci::~xhci() {
    if (dcbaa)
        free(dcbaa);

    if (command_ring)
        free(command_ring);

    if (event_ring)
        free(event_ring);

    if (seg_table)
        free(seg_table);

    if (ports)
        free(ports);
}

void xhci::init_ports() {
    volatile auto& cap_regs = *(xhci_cap_regs*)dev.bar[0].data();
    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)&cap_regs + cap_regs.CAPLENGTH);

    for (unsigned int i = 0; i < num_ports; i++) {
        auto portsc = op_regs.ports[i].PORTSC;

        if (!(portsc & XHCI_PORTSC_CCS)) // ignore if no device connected
            continue;

        // reset port
        portsc &= XHCI_PORTSC_PP | XHCI_PORTSC_PIC;
        portsc |= XHCI_PORTSC_PR;

        op_regs.ports[i].PORTSC = portsc;

        bool port_enabled = false;

        for (unsigned int j = 0; j < 500; j++) {
            if (op_regs.ports[i].PORTSC & XHCI_PORTSC_PED) {
                port_enabled = true;
                break;
            }

            pit_delay(PIT_HZ / 1000); // wait 1ms
        }

        if (!port_enabled) {
            print("timed out waiting for port to be enabled\n");
            continue;
        }
    }
}

void xhci::isr_command_completion(xhci_trb_command_completion& comp) {
    auto ptr = phys_address(comp.command_trb_pointer);

    if (ptr < command_ring || ptr >= command_ring + COMMAND_RING_SIZE)
        return;

    size_t index = (size_t)((uint8_t*)ptr - (uint8_t*)command_ring) / sizeof(xhci_trb_common);
    auto& req = *reqs[index];

    memcpy(&req.comp, &comp, sizeof(comp));

    set_event(static_cast<event&>(req.event.get()));
}

bool xhci::address_device(uint8_t slot, uint8_t port_id) {
    xhci_request req;

    auto& s = *reinterpret_cast<xhci_trb_address_device*>(&req.trb);

    memset(&s, 0, sizeof(s));

    auto context = new xhci_input_context;
    if (!context) {
        print("new failed\n");
        return false;
    }

    memset(context, 0, sizeof(*context));
    context->control.add_context = 3; // slot and EP0

    s.input_context_pointer = va_to_pa(context);
    s.trb_type = (uint16_t)xhci_trb_type::address_device_command;
    s.slot_id = slot;

    context->slot.route_hub_port_number = port_id + 1;
    // FIXME - ep[0]?

    submit(req);

    wait_for_object(req.event.get(), false);

    delete context;

    // FIXME - translate into nt_status?
    if (req.comp.completion_code != 1) {
        print("xhci::address_device: controller returned error {}\n", req.comp.completion_code);
        return false;
    }

    return true;
}

void xhci::port_status_changed(uint8_t port_id) {
    print("xhci::port_status_changed: {}\n", port_id);

    if (port_id == 0 || port_id > num_ports) {
        print("xhci::port_status_changed: port_id {} out of range (expected 1 to {})\n", port_id, num_ports);
        return;
    }

    port_id--;

    volatile auto& op_regs = *(xhci_op_regs*)((uint8_t*)dev.bar[0].data() + caplength);

    bool device_present = op_regs.ports[port_id].PORTSC & XHCI_PORTSC_CCS;

    // ignore spurious messages from controller
    if ((device_present && ports[port_id].slot != 0) || (!device_present && ports[port_id].slot == 0))
        return;

    if (!device_present) {
        // FIXME - disable slot
        // FIXME - if hub, handle child devices
        return;
    }

    auto devctx = new xhci_device_context;
    if (!devctx) {
        print("new failed\n");
        return;
    }

    memset(devctx, 0, sizeof(*devctx));

    auto slot = enable_slot();
    if (slot == 0) {
        print("xhci::port_status_changed: could not enable slot\n");
        delete devctx;
        return;
    }

    ports[port_id].slot = slot;

    print("slot {}\n", slot);

    dcbaa[slot] = va_to_pa(devctx);

    if (!address_device(slot, port_id)) {
        print("xhci::address_device failed\n");
        delete devctx;
        dcbaa[slot] = 0;
        // FIXME - disable slot
        return;
    }

    print("received address {}\n", devctx->slot.usb_device_address);

    // FIXME - get device descriptor etc.
}

void xhci::port_status_change_thread() {
    while (true) {
        wait_for_object(port_status_change_event.get(), true);

        while (true) {
            port_status_change* ent = nullptr;

            {
                lock_guard lock(port_status_change_lock);

                if (!is_list_empty(port_status_change_list)) {
                    ent = &containing_record<port_status_change>(port_status_change_list.next);

                    remove_entry_list(ent->le);
                }
            }

            if (!ent)
                break;

            auto port_id = ent->port_id;
            delete ent;

            port_status_changed(port_id);
        }
    }
}

void xhci::isr_port_status_change(xhci_trb_port_status_change& psc) {
    // FIXME - doing allocation inside ISR isn't brilliant
    auto ent = new port_status_change;
    if (!ent) {
        print("new failed\n");
        return;
    }

    ent->port_id = psc.port_id;

    {
        lock_guard lock(port_status_change_lock);
        insert_tail_list(port_status_change_list, ent->le);
    }

    set_event(static_cast<event&>(port_status_change_event.get()));
}

void xhci::isr() {
    volatile auto& run_regs = *(xhci_run_regs*)(dev.bar[0].data() + rtsoff);
    volatile auto& int_reg = run_regs.int_regs[0];

    while (true) {
        auto& ev = event_ring[event_ring_offset];

        if (ev.trb_type == 0)
            break;

        switch ((enum xhci_trb_type)ev.trb_type) {
            case xhci_trb_type::command_completion_event:
                isr_command_completion(*reinterpret_cast<xhci_trb_command_completion*>(&ev));
                break;

            case xhci_trb_type::port_status_change_event:
                isr_port_status_change(*reinterpret_cast<xhci_trb_port_status_change*>(&ev));
                break;

            default:
                print("xhci::isr: event type {}\n", ev.trb_type); // FIXME?
                break;
        }

        event_ring_offset++;

        if (event_ring_offset == EVENT_RING_SIZE)
            event_ring_offset = 0;
    }

    write_volatile64(&int_reg.ERDP, va_to_pa(&event_ring[event_ring_offset]) | XHCI_ERDP_EHB);
}

void xhci::submit(xhci_request& req) {
    volatile auto& doorbell = *(uint32_t*)(dev.bar[0].data() + dboff);

    req.event = create_event(true);

    {
        lock_guard lock(submit_lock);

        unsigned int index = command_ring_index;

        if (index == 0) {
            auto& cmd_link = *reinterpret_cast<xhci_trb_link*>(&command_ring[COMMAND_RING_SIZE]);
            cmd_link.cycle = cycle_bit ? 1 : 0;
        }

        auto& trb = *(xhci_trb_common*)((uint8_t*)command_ring + (index * sizeof(xhci_trb_common)));

        memcpy(&trb, &req.trb, sizeof(trb));
        trb.cycle = cycle_bit ? 1 : 0;

        command_ring_index++;

        if (command_ring_index == COMMAND_RING_SIZE) {
            command_ring_index = 0;
            cycle_bit = !cycle_bit;
        }

        reqs[index] = &req;

        barrier();

        // FIXME - advance queue pointer?
        doorbell = 0; // FIXME?
    }
}

uint8_t xhci::enable_slot() {
    xhci_request req;

    auto& s = *reinterpret_cast<xhci_trb_enable_slot*>(&req.trb);

    memset(&s, 0, sizeof(s));

    s.slot_type = 0; // FIXME
    s.trb_type = (uint32_t)xhci_trb_type::enable_slot_command;

    submit(req);

    wait_for_object(req.event.get(), false);

    // FIXME - translate into nt_status?
    if (req.comp.completion_code != 1) {
        print("xhci::enable_slot: controller returned error {}\n", req.comp.completion_code);
        return 0;
    }

    return req.comp.slot_id;
}

export
extern "C++"
void init_xhci(pci_device& dev) {
    auto x = new xhci(dev);
    if (!x) {
        print("new failed\n");
        return;
    }

    if (!x->initialized) {
        delete x;
        return;
    }

    x->init_ports();

    // FIXME - add to list of xhci controllers?
}
