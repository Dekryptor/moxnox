#include <stdint.h>
#include <string.h>
#include <mutex>

import nt;
import format;
import print;
import pci;
import list;
import module_init;
import net;
import timer;
import ipv6;
import thread;

using namespace std;

static const uint16_t PCI_VENDOR_ID_REALTEK = 0x10ec;

struct rtl8168_regs {
    uint8_t mac_addr[6];
    uint8_t padding1[2];
    uint32_t multicast_filter[2];
    uint32_t dump_tally_counter_low;
    uint32_t dump_tally_counter_high;
    uint8_t padding2[8];
    uint32_t tx_desc_addr_low;
    uint32_t tx_desc_addr_high;
    uint32_t tx_hpq_desc_addr_low;
    uint32_t tx_hpq_desc_addr_high;
    uint8_t padding3[7];
    uint8_t cmd;
    uint8_t tx_poll;
    uint8_t padding4[3];
    uint16_t interrupt_mask;
    uint16_t interrupt_status;
    uint32_t tx_config;
    uint32_t rx_config;
    uint32_t time_counter_register;
    uint32_t padding5;
    uint8_t eeprom_command_register;
    uint8_t config0;
    uint8_t config1;
    uint8_t config2;
    uint8_t config3;
    uint8_t config4;
    uint8_t config5;
    uint8_t padding6;
    uint32_t timer_interrupt;
    uint8_t padding7[4];
    uint32_t phy_access;
    uint8_t padding8[12];
    uint32_t phy_status;
    uint8_t padding9[108];
    uint16_t rx_max_size;
    uint8_t padding10[2];
    uint32_t rx_desc_addr_low;
    uint32_t rx_desc_addr_high;
    uint8_t padding11[20];
} __attribute__((packed));

static_assert(sizeof(rtl8168_regs) == 0x100);

static const uint8_t RTL_CMD_RESET = 0x10;
static const uint8_t RTL_CMD_RX_ENABLE = 0x08;
static const uint8_t RTL_CMD_TX_ENABLE = 0x04;

static const uint16_t DEFAULT_MTU = 1500; // FIXME - put in net module?

class rtl8168_netif : public netif {
public:
    rtl8168_netif(pci_device& pci) : pci(pci) { }

    void recv_thread();
    nt_status send_packet(const mac_address& /*dst_mac*/, ethertype /*type*/,
                          packet& /*pack*/) override {
        print("FIXME - rtl8168_netif::send_packet\n");

        return nt_status::NOT_IMPLEMENTED;
    }

    pci_device& pci;
};

void found_rtl8168(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REALTEK, 0x8168, true, found_rtl8168 },
};

static void init_rtl8168() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }
}

static module_init module_init_rtl8168(init_rtl8168);

void found_rtl8168(pci_device& dev) {
    print("FIXME - found_rtl8168\n"); // FIXME

    // FIXME - is it definitely always BAR2? What does Linux do?

    if (dev.bar[2].empty()) {
        print("found_rtl8168: BAR2 not set\n");
        return;
    }

    if (dev.bar[2].size() < sizeof(rtl8168_regs)) {
        print("found_rtl8168: BAR2 was {} bytes, expected at least {}\n",
              dev.bar[2].size(), sizeof(rtl8168_regs));
        return;
    }

    volatile auto& regs = *(rtl8168_regs*)dev.bar[2].data();
    bool reset_done = false;

    regs.cmd = RTL_CMD_RESET;

    // wait up to 100ms
    for (unsigned int i = 0; i < 100; i++) {
        if (!(regs.cmd & RTL_CMD_RESET)) {
            reset_done = true;
            break;
        }

        msleep(1);
    }

    if (!reset_done) {
        print("found_rtl8168: card unresponsive, reset did not complete\n");
        return;
    }

    mac_address mac;

    for (unsigned int i = 0; i < 6; i++) {
        mac.v[i] = regs.mac_addr[i];
    }

    auto pc = new rtl8168_netif(dev);
    if (!pc) {
        print("new failed\n");
        return;
    }

    auto& c = *pc;

    memcpy(&c.mac, &mac, sizeof(mac));
    c.mtu = DEFAULT_MTU; // FIXME?

    c.ll_addr = find_ll_address(mac);

    auto a = new ip_addr;
    if (!a) {
        print("new failed\n");
        delete pc;
        return;
    }

    a->addr = c.ll_addr;

    {
        lock_guard lg(c.addr_lock);
        insert_tail_list(c.addrs, a->le);
    }

    print("found rtl8168 device with MAC {}, MTU {}, link-local address {}\n",
          c.mac, c.mtu, c.ll_addr);

    c.num = next_netif_num.fetch_add(1);

    auto eth_num = next_eth_num.fetch_add(1);

    {
        char s[50];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "eth{}", eth_num);

        auto sv = string_view(s, (size_t)(sp.data() - s));

        memcpy(c.name, sv.data(), sv.size());
        memset(c.name + sv.size(), 0, sizeof(c.name) - sv.size());
    }

    {
        lock_guard lg(netif_lock);
        insert_tail_list(netif_list, c.le);
    }

    c.acquire();

    add_kernel_thread([](void* ctx) {
        auto& dev = *(rtl8168_netif*)ctx;

        dev.recv_thread();
        dev.release();
    }, &c);
}

void rtl8168_netif::recv_thread() {
    print("FIXME - rtl8168_netif::recv_thread\n"); // FIXME
}
