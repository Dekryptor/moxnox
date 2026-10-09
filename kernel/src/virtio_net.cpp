#include <span>
#include <string_view>
#include <bit>
#include <stddef.h>
#include <stdint.h>
#include <mutex>
#include <variant>
#include <atomic>

import nt;
import print;
import format;
import pci;
import virtio;
import list;
import module_init;
import thread;
import handle;
import object_ref;
import net;
import mem;
import spinlock;
import dma;
import crt;
import ipv4;
import ipv6;
import bsod;

using namespace std;

// FIXME - keep in sync with one in net
static constexpr size_t CACHE_LINE = 64;

static const uint64_t VIRTIO_NET_F_CSUM = 1 << 0;
static const uint64_t VIRTIO_NET_F_GUEST_CSUM = 1 << 1;
static const uint64_t VIRTIO_NET_F_CTRL_GUEST_OFFLOADS = 1 << 2;
static const uint64_t VIRTIO_NET_F_MTU = 1 << 3;
static const uint64_t VIRTIO_NET_F_MAC = 1 << 5;
static const uint64_t VIRTIO_NET_F_GUEST_TSO4 = 1 << 7;
static const uint64_t VIRTIO_NET_F_GUEST_TSO6 = 1 << 8;
static const uint64_t VIRTIO_NET_F_GUEST_ECN = 1 << 9;
static const uint64_t VIRTIO_NET_F_GUEST_UFO = 1 << 10;
static const uint64_t VIRTIO_NET_F_HOST_TSO4 = 1 << 11;
static const uint64_t VIRTIO_NET_F_HOST_TSO6 = 1 << 12;
static const uint64_t VIRTIO_NET_F_HOST_ECN = 1 << 13;
static const uint64_t VIRTIO_NET_F_HOST_UFO = 1 << 14;
static const uint64_t VIRTIO_NET_F_MRG_RXBUF = 1 << 15;
static const uint64_t VIRTIO_NET_F_STATUS = 1 << 16;
static const uint64_t VIRTIO_NET_F_CTRL_VQ = 1 << 17;
static const uint64_t VIRTIO_NET_F_CTRL_RX = 1 << 18;
static const uint64_t VIRTIO_NET_F_CTRL_VLAN = 1 << 19;
static const uint64_t VIRTIO_NET_F_CTRL_RX_EXTRA = 1 << 20;
static const uint64_t VIRTIO_NET_F_GUEST_ANNOUNCE = 1 << 21;
static const uint64_t VIRTIO_NET_F_MQ = 1 << 22;
static const uint64_t VIRTIO_NET_F_CTRL_MAC_ADDR = 1 << 23;
static const uint64_t VIRTIO_NET_F_HASH_TUNNEL = (uint64_t)1 << 51;
static const uint64_t VIRTIO_NET_F_VQ_NOTF_COAL = (uint64_t)1 << 52;
static const uint64_t VIRTIO_NET_F_NOTF_COAL = (uint64_t)1 << 53;
static const uint64_t VIRTIO_NET_F_GUEST_USO4 = (uint64_t)1 << 54;
static const uint64_t VIRTIO_NET_F_GUEST_USO6 = (uint64_t)1 << 55;
static const uint64_t VIRTIO_NET_F_HOST_USO = (uint64_t)1 << 56;
static const uint64_t VIRTIO_NET_F_HASH_REPORT = (uint64_t)1 << 57;
static const uint64_t VIRTIO_NET_F_GUEST_HDRLEN = (uint64_t)1 << 59;
static const uint64_t VIRTIO_NET_F_RSS = (uint64_t)1 << 60;
static const uint64_t VIRTIO_NET_F_RSC_EXT = (uint64_t)1 << 61;
static const uint64_t VIRTIO_NET_F_STANDBY = (uint64_t)1 << 62;
static const uint64_t VIRTIO_NET_F_SPEED_DUPLEX = (uint64_t)1 << 63;

static const uint64_t SUPPORTED_FEATURES = VIRTIO_F_VERSION_1 | VIRTIO_NET_F_MTU | \
                                           VIRTIO_NET_F_MAC;

static const uint16_t DEFAULT_MTU = 1500;

struct virtio_net_config {
    uint8_t mac[6];
    uint16_t status;
    uint16_t max_virtqueue_pairs;
    uint16_t mtu;
    uint32_t speed;
    uint8_t duplex;
    uint8_t rss_max_key_size;
    uint16_t rss_max_indirection_table_length;
    uint32_t supported_hash_types;
    uint32_t supported_tunnel_types;
} __attribute((packed));

static_assert(sizeof(virtio_net_config) == 28);

struct virtio_net_hdr {
    uint8_t flags;
    uint8_t gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute((packed));

// FIXME - longer if VIRTIO_NET_F_HASH_REPORT
static_assert(sizeof(virtio_net_hdr) == 12);

static const uint8_t VIRTIO_NET_HDR_F_NEEDS_CSUM = 1;
static const uint8_t VIRTIO_NET_HDR_F_DATA_VALID = 2;
static const uint8_t VIRTIO_NET_HDR_F_RSC_INFO = 4;

static const uint8_t VIRTIO_NET_HDR_GSO_NONE = 0;
static const uint8_t VIRTIO_NET_HDR_GSO_TCPV4 = 1;
static const uint8_t VIRTIO_NET_HDR_GSO_UDP = 3;
static const uint8_t VIRTIO_NET_HDR_GSO_TCPV6 = 4;
static const uint8_t VIRTIO_NET_HDR_GSO_UDP_L4 = 5;
static const uint8_t VIRTIO_NET_HDR_GSO_ECN = 0x80;

static const uint16_t PCI_VENDOR_ID_REDHAT = 0x1af4;

void found_virtio_pci_net(pci_device& dev);

static pci_recognizer mod_pci_ids[] = {
    { {}, PCI_VENDOR_ID_REDHAT, 0x1000, true, found_virtio_pci_net },
    { {}, PCI_VENDOR_ID_REDHAT, 0x1041, true, found_virtio_pci_net },
};

static bool found_virtio_mmio_net(virtio_mmio_device& v);

static virtio_mmio_recognizer mod_virtio_mmio_types = {
    {}, 1, SUPPORTED_FEATURES, 2, found_virtio_mmio_net
};

static void init_virtio_pci() {
    for (unsigned int i = 0; i < sizeof(mod_pci_ids) / sizeof(*mod_pci_ids); i++) {
        insert_tail_list(pci_ids, mod_pci_ids[i].le);
    }

    insert_tail_list(virtio_mmio_types, mod_virtio_mmio_types.le);
}

static module_init module_init_virtio_net(init_virtio_pci);

template<virtio_device T>
class virtio_net : public netif {
public:
    virtio_net(T& t);

    virtual ~virtio_net() {
        delete &dev;
    }

    void init(const virtio_net_config& cfg);
    void recv_thread();
    nt_status send_packet(const mac_address& dst_mac, ethertype type,
                          packet& pack) override;

    T& dev;

private:
    virt_queue* receive_queue = nullptr;
    virt_queue* transmit_queue = nullptr;
};

#if 0
static void test_ipv6_address_printing() {
    print("{}\n", ipv6_address{0x20,0x01,0x0d,0xb8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01}); // "2001:db8::1"
    print("{}\n", ipv6_address{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1}); // "::1"
    print("{}\n", ipv6_address{0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}); // "::"
    print("{}\n", ipv6_address{0x20,0x01,0x0d,0xb8,0x00,0x01,0x00,0x02,0x00,0x03,0x00,0x04,0x00,0x05,0x00,0x06}); // "2001:db8:1:2:3:4:5:6"
    print("{}\n", ipv6_address{0xfe,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0xfc,0x4f,0x5f,0xff,0xfe,0x00,0xe1,0x60}); // "fe80::fc4f:5fff:fe00:e160"
    print("{}\n", ipv6_address{0xfe,0x80,0x00,0x00,0x00,0x00,0x00,0x00,0x56,0x54,0x4d,0xff,0xfe,0x78,0x4e,0x78}); // "fe80::5654:4dff:fe78:4e78"
    print("{}\n", ipv6_address{0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01}); // "ff02::1"
    print("{}\n", ipv6_address{0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x02}); // "ff02::2"
    print("{}\n", ipv6_address{0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x16}); // "ff02::16"
    print("{}\n", ipv6_address{0xff,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0xff,0x78,0x4e,0x78}); // "ff02::1:ff78:4e78"
    print("{}\n", ipv6_address{0x20,0x01,0x0d,0xb8,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}); // "2001:db8::"
    print("{}\n", ipv6_address{0x20,0x01,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x02,0x00,0x00,0x00,0x00,0x00,0x01}); // "2001::1:2:0:0:1"
    print("{}\n", ipv6_address{0x20,0x01,0x0d,0xb8,0x00,0x01,0x00,0x00,0x00,0x03,0x00,0x04,0x00,0x05,0x00,0x06}); // "2001:db8:1:0:3:4:5:6"
    print("{}\n", ipv6_address{0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,0xc0,0x00,0x02,0x01}); // "::ffff:192.0.2.1"
}
#endif

template<virtio_device T>
void virtio_net<T>::init(const virtio_net_config& cfg) {
    memcpy(&mac, cfg.mac, sizeof(mac));

    if (dev.negotiated_features & VIRTIO_NET_F_MTU)
        mtu = cfg.mtu;
    else
        mtu = DEFAULT_MTU;

#if 0
    print("virtio_net cfg: mac = {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}, status = {}, max_virtqueue_pairs = {}, mtu = {}, speed = {}, duplex = {}, rss_max_key_size = {}, rss_max_indirection_table_length = {}, supported_hash_types = {}, supported_tunnel_types = {}\n",
          cfg.mac[0], cfg.mac[1], cfg.mac[2], cfg.mac[3], cfg.mac[4], cfg.mac[5],
          cfg.status, cfg.max_virtqueue_pairs, cfg.mtu, cfg.speed, cfg.duplex,
          cfg.rss_max_key_size, cfg.rss_max_indirection_table_length,
          cfg.supported_hash_types, cfg.supported_tunnel_types);
#endif

    //test_ipv6_address_printing();

    ll_addr = find_ll_address(mac);

    auto a = new ip_addr(ll_addr);
    if (!a) {
        print("new failed\n");
        return;
    }

    {
        lock_guard lg(addr_lock);
        insert_tail_list(addrs, a->le);
    }

    print("found virtio-net device with MAC {}, MTU {}, link-local address {}\n",
          mac, mtu, ll_addr);

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            receive_queue = &vq;
        else if (vq.num == 1)
            transmit_queue = &vq;
    }

    this->num = next_netif_num.fetch_add(1);

    auto eth_num = next_eth_num.fetch_add(1);

    {
        char s[50];
        auto sp = span(s, sizeof(s));

        sp = format(sp, "eth{}", eth_num);

        auto sv = string_view(s, (size_t)(sp.data() - s));

        memcpy(this->name, sv.data(), sv.size());
        memset(this->name + sv.size(), 0, sizeof(this->name) - sv.size());
    }

    {
        lock_guard lg(netif_lock);
        insert_tail_list(netif_list, this->le);
    }

    this->acquire();

    auto t = add_kernel_thread([](void* ctx) {
        auto& dev = *(virtio_net<virtio_pci_device>*)ctx;

        dev.recv_thread();
        dev.release();
    }, this);

    if (!t) {
        print("add_kernel_thread failed\n");
        this->release();
        return;
    }
}

template<virtio_device T>
virtio_net<T>::virtio_net(T& dev) : dev(dev) {
    if constexpr (is_same_v<T, virtio_pci_device>) {
        if (dev.device_cfg->length < sizeof(virtio_net_config)) {
            print("virtio_net: device cfg length was {}, expected at least {}\n",
                +dev.device_cfg->length, sizeof(virtio_net_config));
            return;
        }
    }

    if (!(dev.negotiated_features & VIRTIO_NET_F_MAC)) {
        // FIXME - generate one in this case?
        print("virtio_net: device did not provide a MAC address\n");
        return;
    }

    if (!(dev.negotiated_features & VIRTIO_F_VERSION_1)) {
        print("virtio-net: legacy devices not supported\n");
        return;
    }

    if constexpr (is_same_v<T, virtio_pci_device>) {
        init(*(virtio_net_config*)(dev.dev.bar[dev.device_cfg->bar].data() + dev.device_cfg->offset));
    } else if constexpr (is_same_v<T, virtio_mmio_device>) {
        init(*(virtio_net_config*)dev.config);
    }
}

void found_virtio_pci_net(pci_device& dev) {
    auto d = new virtio_pci_device(dev, SUPPORTED_FEATURES, 2);
    if (!d) {
        print("new failed\n");
        return;
    }

    auto pv = new virtio_net(*d);
    if (!pv) {
        print("new failed\n");
        delete d;
        return;
    }

    auto& v = *pv;

    if (!v.dev.initialized) {
        delete pv;
        return;
    }

    v.dev.finalize();
}

static bool found_virtio_mmio_net(virtio_mmio_device& dev) {
    auto pv = new virtio_net(dev);
    if (!pv) {
        print("new failed\n");
        return false;
    }

    auto& v = *pv;

    if (!v.dev.initialized) {
        delete pv;
        return false;
    }

    v.dev.finalize();

    return true;
}

template<virtio_device T>
nt_status virtio_net<T>::send_packet(const mac_address& dst_mac, ethertype type,
                                     packet& pack) {
    uint8_t* bounce1 = nullptr;
    uint8_t* bounce2 = nullptr;
    virtq_desc descs[2];

    struct {
        virtio_net_hdr hdr;
        ethernet_header eth;
    } __attribute((packed)) header;

    header.hdr.flags = 0;
    header.hdr.gso_type = VIRTIO_NET_HDR_GSO_NONE;
    header.hdr.hdr_len = 0;
    header.hdr.gso_size = 0;
    header.hdr.csum_start = 0;
    header.hdr.csum_offset = 0;
    header.hdr.num_buffers = 0;

    header.eth.src_mac = mac;
    header.eth.dst_mac = dst_mac;
    header.eth.type = type;

    auto pa1 = dma_setup(span((uint8_t*)&header, sizeof(header)), true, bounce1);
    if (!pa1) {
        print("dma_setup failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto pa2 = dma_setup_no_bounce(span(pack.data, PACKET_DATA_SIZE_PLUS_PADDING));

    descs[0].addr = pa1;
    descs[0].len = sizeof(header);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = pa2;
    descs[1].len = (uint32_t)pack.size;
    descs[1].flags = 0;
    descs[1].next = 0;

    auto pa3 = dma_setup(span((uint8_t*)descs, sizeof(virtq_desc) * 2),
                         true, bounce2);
    if (!pa3) {
        dma_finish_write(bounce1);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto slot = transmit_queue->async_indirect(pa3, sizeof(virtq_desc) * 2);

    transmit_queue->wait_read(slot);

    dma_finish_write(bounce2);
    dma_finish_write(bounce1);

    tx_packets.fetch_add(1, memory_order::relaxed);
    tx_bytes.fetch_add(sizeof(ethernet_header) + pack.size, memory_order::relaxed);

    return nt_status::SUCCESS;
}

struct alignas(CACHE_LINE) virtio_net_recv_entry {
    virtio_net_hdr hdr;
    ethernet_header eth;
    uint8_t padding[CACHE_LINE - sizeof(virtio_net_hdr) - sizeof(ethernet_header)];
    uint8_t* bounce1;
    virtq_desc descs[2];
    packet* pack;
    uint8_t slot;
};

static constexpr size_t ENTRY_HEADER_SIZE_PLUS_PADDING = offsetof(virtio_net_recv_entry, bounce1);

static_assert(offsetof(virtio_net_recv_entry, hdr) % CACHE_LINE == 0);
static_assert(ENTRY_HEADER_SIZE_PLUS_PADDING % CACHE_LINE == 0);

template<virtio_device T>
void virtio_net<T>::recv_thread() {
    // FIXME - die gracefully if device ever goes away

    static const size_t BUFFER_LENGTH = 16;

    virtio_net_recv_entry entries[BUFFER_LENGTH];

    auto init_entry = [this](virtio_net_recv_entry& e) -> bool {
        e.bounce1 = nullptr;

        e.pack = new packet;
        if (!e.pack) {
            print("new failed\n");
            return false;
        }

        auto pa1 = dma_setup_no_bounce(span((uint8_t*)&e.hdr, ENTRY_HEADER_SIZE_PLUS_PADDING));
        auto pa2 = dma_setup_no_bounce(span((uint8_t*)&e.pack->data, PACKET_DATA_SIZE_PLUS_PADDING));

        e.descs[0].addr = pa1;
        e.descs[0].len = sizeof(virtio_net_hdr) + sizeof(ethernet_header);
        e.descs[0].flags = VIRTQ_DESC_F_WRITE | VIRTQ_DESC_F_NEXT;
        e.descs[0].next = 1;

        e.descs[1].addr = pa2;
        e.descs[1].len = (uint32_t)mtu;
        e.descs[1].flags = VIRTQ_DESC_F_WRITE;
        e.descs[1].next = 0;

        auto pa3 = dma_setup(span((uint8_t*)e.descs, sizeof(virtq_desc) * 2), true, e.bounce1);
        if (!pa3) {
            print("dma_setup failed\n");
            dma_finish_read_no_bounce(span((uint8_t*)&e.pack->data, PACKET_DATA_SIZE_PLUS_PADDING));
            dma_finish_read_no_bounce(span((uint8_t*)&e.hdr, ENTRY_HEADER_SIZE_PLUS_PADDING));
            e.pack->release();
            return false;
        }

        e.slot = receive_queue->async_indirect(pa3, sizeof(virtq_desc) * 2);

        return true;
    };

    for (size_t i = 0; i < BUFFER_LENGTH; i++) {
        if (!init_entry(entries[i]))
            bsod("failed to initialize virtio_net buffer entry");
    }

    for (size_t idx = 0; ; idx = (idx + 1) % BUFFER_LENGTH) {
        auto& e = entries[idx];

        auto recvd_bytes = receive_queue->wait_read(e.slot);

        dma_finish_write(e.bounce1);

        dma_finish_read_no_bounce(span((uint8_t*)&e.hdr, ENTRY_HEADER_SIZE_PLUS_PADDING));
        dma_finish_read_no_bounce(span((uint8_t*)&e.pack->data, PACKET_DATA_SIZE_PLUS_PADDING));

        if (recvd_bytes < sizeof(virtio_net_hdr) + sizeof(ethernet_header)) {
            print("virtio_net::recv_thread: received {} bytes, expected at least {}\n",
                  recvd_bytes, sizeof(virtio_net_hdr) + sizeof(ethernet_header));
            e.pack->release();

            if (!init_entry(e))
                bsod("failed to initialize virtio_net buffer entry");

            continue;
        }

#if 0
        print("recvd_bytes = {}\n", recvd_bytes);
        print("hdr: flags = {}, gso_type = {}, hdr_len = {}, gso_size = {}, csum_start = {}, csum_offset = {}, num_buffers = {}\n",
              e.hdr.flags, e.hdr.gso_type, +e.hdr.hdr_len,
              +e.hdr.gso_size, +e.hdr.csum_start, +e.hdr.csum_offset,
              +e.hdr.num_buffers);
        print("ethernet: dst_mac = {}, src_mac = {}, type = {}\n",
              e.eth.dst_mac, e.eth.src_mac, *&e.eth.type);
#endif

        rx_packets.fetch_add(1, memory_order::relaxed);
        rx_bytes.fetch_add(recvd_bytes - sizeof(virtio_net_hdr),
                           memory_order::relaxed);

        if (drop_packet(e.eth.dst_mac, mac)) {
            e.pack->release();

            if (!init_entry(e))
                bsod("failed to initialize virtio_net buffer entry");

            continue;
        }

        e.pack->size = recvd_bytes - sizeof(virtio_net_hdr) - sizeof(ethernet_header);

        received_packet(*this, *e.pack, e.eth.src_mac, e.eth.type, false);

        e.pack->release();

        if (!init_entry(e))
            bsod("failed to initialize virtio_net buffer entry");
    }
}
