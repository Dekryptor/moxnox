#include <stdint.h>
#include <string.h>
#include <span>
#include <string_view>
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
import mem;
import interrupts;

using namespace std;

static const uint16_t PCI_VENDOR_ID_REALTEK = 0x10ec;

union tcr_bits {
    uint32_t value;
    struct {
        uint32_t : 4;
        uint32_t tx_retry_count : 4;
        uint32_t dma_burst : 3;
        uint32_t : 5;
        uint32_t crc_append : 1;
        uint32_t loopback : 2;
        uint32_t : 5;
        uint32_t ifg : 2;
        uint32_t : 6;
    };
};

union rcr_bits {
    uint32_t value;
    struct {
        uint32_t accept_phys_match : 1;
        uint32_t accept_phys : 1;
        uint32_t accept_multicast : 1;
        uint32_t accept_broadcast : 1;
        uint32_t accept_runt : 1;
        uint32_t accept_error : 1;
        uint32_t : 1;
        uint32_t wrap : 1;
        uint32_t rx_dma_burst : 3;
        uint32_t rx_buf_size : 2;
        uint32_t rx_fifo_thresh : 3;
        uint32_t : 6;
        uint32_t vlan_strip : 1;
        uint32_t : 1;
        uint32_t max_dma : 3;
        uint32_t : 4;
        uint32_t accept_all : 1;
    };
} __attribute__((packed));

union int_bits {
    uint16_t value;
    struct {
        uint16_t rx_ok : 1;
        uint16_t rx_err : 1;
        uint16_t tx_ok : 1;
        uint16_t tx_err : 1;
        uint16_t rx_desc_unavail : 1;
        uint16_t link_change : 1;
        uint16_t rx_fifo_overflow : 1;
        uint16_t tx_desc_unavail : 1;
        uint16_t sw_int : 1;
        uint16_t rx_fifo_empty_under_fifo_full: 1;
        uint16_t : 4;
        uint16_t timeout : 1;
        uint16_t : 1;
    };
} __attribute__((packed));

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
    int_bits interrupt_mask;
    int_bits interrupt_status;
    tcr_bits tx_config;
    rcr_bits rx_config;
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

struct rtl8168_rx_desc {
    union {
        uint32_t opts1;
        struct {
            uint32_t packet_length : 14;
            uint32_t : 1;
            uint32_t tcp_checksum_failed : 1;
            uint32_t udp_checksum_failed : 1;
            uint32_t ip_checksum_failed : 1;
            uint32_t : 4;
            uint32_t tcpv6 : 1;
            uint32_t udpv6 : 1;
            uint32_t ipv6 : 1;
            uint32_t tcpv4 : 1;
            uint32_t udpv4 : 1;
            uint32_t ipv4 : 1;
            uint32_t last_segment : 1;
            uint32_t first_segment : 1;
            uint32_t end_of_ring : 1;
            uint32_t owned_by_nic : 1;
        };
    };

    union {
        uint32_t opts2;
        struct {
            uint32_t vlan_id : 16;
            uint32_t vlan_tagged : 1;
            uint32_t : 15;
        };
    };

    uint64_t addr;
} __attribute__((packed));

static_assert(sizeof(rtl8168_rx_desc) == 0x10);

static const uint8_t RTL_CMD_RESET = 0x10;
static const uint8_t RTL_CMD_RX_ENABLE = 0x08;
static const uint8_t RTL_CMD_TX_ENABLE = 0x04;

static const uint16_t DEFAULT_MTU = 1500; // FIXME - put in net module?

static const size_t NUM_RX_DESC = 256;
static const uint16_t RX_BUF_SIZE = 2048;

static const uint8_t RTL_CFG9346_UNLOCK = 0xc0;
static const uint8_t RTL_CFG9346_LOCK = 0x00;

class rtl8168_netif : public netif {
public:
    rtl8168_netif(pci_device& dev, volatile rtl8168_regs& regs) : dev(dev), regs(regs) { }

    void recv_thread();
    nt_status send_packet(const mac_address& /*dst_mac*/, ethertype /*type*/,
                          packet& /*pack*/) override {
        print("FIXME - rtl8168_netif::send_packet\n");

        return nt_status::NOT_IMPLEMENTED;
    }

    void init();
    bool isr();

    pci_device& dev;
    volatile rtl8168_regs& regs;

private:
    void setup_descriptors();
    void enable_interrupts();
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

void rtl8168_netif::setup_descriptors() {
    auto rx_ring = (rtl8168_rx_desc*)malloc_aligned(NUM_RX_DESC * sizeof(rtl8168_rx_desc), 256);
    // FIXME - check for OOM

    memset(rx_ring, 0, NUM_RX_DESC * sizeof(rtl8168_rx_desc));

    for (size_t i = 0; i < NUM_RX_DESC; i++) {
        void* rx_buf = malloc(RX_BUF_SIZE); // FIXME - check for OOM

        rx_ring[i].addr = va_to_pa_phys(rx_buf);
        rx_ring[i].owned_by_nic = 1;
        rx_ring[i].packet_length = RX_BUF_SIZE;

        if (i == NUM_RX_DESC - 1)
            rx_ring[i].end_of_ring = 1;
    }

    auto rx_phys = va_to_pa_phys(rx_ring);

    regs.rx_desc_addr_low = (uint32_t)rx_phys;
    regs.rx_desc_addr_high = (uint32_t)(rx_phys >> 32);
}

void rtl8168_netif::enable_interrupts() {
    regs.interrupt_status.value = 0xffff; // clear pending

    int_bits b;

    b.value = 0;
    b.rx_ok = 1;
    b.tx_ok = 1;
    b.rx_err = 1;
    b.tx_err = 1;
    b.link_change = 1;
    b.rx_fifo_overflow = 1;

    regs.interrupt_mask.value = b.value;
}

bool rtl8168_netif::isr() {
    print("FIXME - rtl8168_netif::isr\n");

    return false;
}

void rtl8168_netif::init() {
    tcr_bits tx_config;
    rcr_bits rx_config;

    regs.eeprom_command_register = RTL_CFG9346_UNLOCK;

    tx_config.value = 0;
    tx_config.ifg = 3; // standard IFG
    tx_config.dma_burst = 7; // unlimited

    regs.tx_config.value = tx_config.value;

    rx_config.value = 0;
    rx_config.accept_phys = 1; // our MAC
    rx_config.accept_multicast = 1; // multicast
    rx_config.accept_broadcast = 1; // broadcast
    rx_config.rx_dma_burst = 7; // unlimited
    rx_config.rx_fifo_thresh = 7; // no threshold
    rx_config.rx_buf_size = 1; // 16K
    rx_config.accept_all = 1; // FIXME (promiscuous mode)
    rx_config.accept_error = 1; // FIXME

    regs.rx_config.value = rx_config.value;

    regs.rx_max_size = RX_BUF_SIZE;

    setup_descriptors();

    // FIXME - MSI

    if (!add_irq_handler(*dev.irq, true, [](void* ctx) { return ((rtl8168_netif*)ctx)->isr(); }, this)) {
        print("add_irq_handler failed\n");
        return;
    }

    enable_interrupts();

    regs.cmd = RTL_CMD_RX_ENABLE;

    regs.eeprom_command_register = RTL_CFG9346_LOCK;
}

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

    auto pc = new rtl8168_netif(dev, regs);
    if (!pc) {
        print("new failed\n");
        return;
    }

    auto& c = *pc;

    c.init();

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
