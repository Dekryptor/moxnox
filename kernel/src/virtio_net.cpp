#include <span>
#include <atomic>
#include <string_view>
#include <bit>

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

using namespace std;

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

struct ip_addr {
    list_entry le;
    ipv6_address addr;
};

class network_interface {
public:
    virtual nt_status send_packet(span<const uint8_t> buf) = 0;

    mac_address mac;
    uint16_t mtu;

    spinlock addr_lock;
    list_entry addrs = { &addrs, &addrs };
};

template<typename T>
class virtio_net : public network_interface {
public:
    virtio_net(pci_device& d);
    virtio_net(T& t);

    virtual ~virtio_net() {
        delete &dev;
    }

    void init(const virtio_net_config& cfg);
    void recv_thread();
    nt_status send_packet(span<const uint8_t> buf) override;

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

static ipv6_address find_ll_address(const mac_address& m) {
    ipv6_address ret;

    ret.v[0] = 0xfe;
    ret.v[1] = 0x80;
    ret.v[2] = 0x00;
    ret.v[3] = 0x00;
    ret.v[4] = 0x00;
    ret.v[5] = 0x00;
    ret.v[6] = 0x00;
    ret.v[7] = 0x00;
    ret.v[8] = m.v[0] ^ 0x02;
    ret.v[9] = m.v[1];
    ret.v[10] = m.v[2];
    ret.v[11] = 0xff;
    ret.v[12] = 0xfe;
    ret.v[13] = m.v[3];
    ret.v[14] = m.v[4];
    ret.v[15] = m.v[5];

    return ret;
}

template<typename T>
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

    auto ll = find_ll_address(mac);

    auto a = new ip_addr;
    if (!a) {
        print("new failed\n");
        return;
    }

    a->addr = ll;

    {
        lock_guard lg(addr_lock);
        insert_tail_list(addrs, a->le);
    }

    print("found virtio-net device with MAC {}, MTU {}, link-local address {}\n",
          mac, mtu, ll);

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            receive_queue = &vq;
        else if (vq.num == 1)
            transmit_queue = &vq;
    }

    add_kernel_thread([](void* ctx) {
        ((virtio_net<virtio_pci_device>*)ctx)->recv_thread();
    }, this);
}

template<>
virtio_net<virtio_pci_device>::virtio_net(pci_device& d) : dev(*new virtio_pci_device(d, SUPPORTED_FEATURES, 2)) {
    if (dev.device_cfg->length < sizeof(virtio_net_config)) {
        print("virtio_net: device cfg length was {}, expected at least {}\n",
              dev.device_cfg->length, sizeof(virtio_net_config));
        return;
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

    init(*(virtio_net_config*)(d.bar[dev.device_cfg->bar].data() + dev.device_cfg->offset));
}

template<>
virtio_net<virtio_mmio_device>::virtio_net(virtio_mmio_device& d) : dev(d) {
    if (!(dev.negotiated_features & VIRTIO_NET_F_MAC)) {
        // FIXME - generate one in this case?
        print("virtio_net: device did not provide a MAC address\n");
        return;
    }

    if (!(dev.negotiated_features & VIRTIO_F_VERSION_1)) {
        print("virtio-net: legacy devices not supported\n");
        return;
    }

    init(*(virtio_net_config*)dev.config);
}

static atomic<uint32_t> virtio_net_num = 0;

void found_virtio_pci_net(pci_device& dev) {
    auto pv = new virtio_net<virtio_pci_device>(dev);
    if (!pv) {
        print("new failed\n");
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
    auto pv = new virtio_net<virtio_mmio_device>(dev);
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

template<typename T>
nt_status virtio_net<T>::send_packet(span<const uint8_t> buf) {
    virtio_net_hdr hdr;
    uint8_t* bounce1 = nullptr;
    uint8_t* bounce2 = nullptr;
    virtq_desc descs[2];
    uint64_t read_bytes;

    hdr.flags = 0;
    hdr.gso_type = VIRTIO_NET_HDR_GSO_NONE;
    hdr.hdr_len = 0;
    hdr.gso_size = 0;
    hdr.csum_start = 0;
    hdr.csum_offset = 0;
    hdr.num_buffers = 0;

    auto pa1 = dma_setup(span((uint8_t*)&hdr, sizeof(hdr)), true, bounce1);
    if (!pa1) {
        print("dma_setup failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    auto pa2 = dma_setup(buf, true, bounce2);
    if (!pa2) {
        print("dma_setup failed\n");
        dma_finish_write(bounce1);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    descs[0].addr = pa1;
    descs[0].len = sizeof(virtio_net_hdr);
    descs[0].flags = VIRTQ_DESC_F_NEXT;
    descs[0].next = 1;

    descs[1].addr = pa2;
    descs[1].len = (uint32_t)buf.size();
    descs[1].flags = 0;
    descs[1].next = 0;

    auto status = transmit_queue->indirect(descs, read_bytes);

    dma_finish_write(bounce1);
    dma_finish_write(bounce2);

    return status;
}

static void handle_neighbour_solicitation(network_interface& netif,
                                          span<const uint8_t> buf) {
    if (buf.size() < sizeof(icmpv6_neighbour_solicitation)) {
        print("handle_neighbour_solicitation: buffer length was {}, expected at least {}\n",
              buf.size(), sizeof(icmpv6_neighbour_solicitation));
        return;
    }

    auto& ns = *(icmpv6_neighbour_solicitation*)buf.data();

    print("ns: target = {}\n", ns.target);

    // FIXME - options (sender's MAC means we can avoid sending NS back to them)

    bool found = false;
    {
        lock_guard lock(netif.addr_lock);

        for (auto le = netif.addrs.next; le != &netif.addrs; le = le->next) {
            auto& a = containing_record<ip_addr>(le);

            if (a.addr == ns.target) {
                found = true;
                break;
            }
        }
    }

    if (!found)
        return;

    print("FIXME - respond to neighbour solicitation\n");
}

static void handle_icmpv6_packet(network_interface& netif, span<const uint8_t> buf) {
    if (buf.size() < sizeof(icmpv6_header)) {
        print("handle_icmpv6_packet: buffer length was {}, expected at least {}\n",
              buf.size(), sizeof(icmpv6_header));
        return;
    }

    auto& header = *(icmpv6_header*)buf.data();

    print("icmpv6: type = {}, code = {:02x}, checksum = {:04x}\n",
          header.type, header.code, byteswap(header.checksum));

    switch (header.type) {
        case icmpv6_type::neighbour_solicitation:
            handle_neighbour_solicitation(netif, buf);
            break;

        default:
            break;
    }
}

static void handle_ipv6_packet(network_interface& netif, span<const uint8_t> buf) {
    if (buf.size() < sizeof(ipv6_header)) {
        print("handle_ipv6_packet: buffer length was {}, expected at least {}\n",
              buf.size(), sizeof(ipv6_header));
        return;
    }

    auto& header = *(ipv6_header*)buf.data();

    print("ipv6: version = {}, traffic_class1 = {}, traffic_class2 = {}, flow_label = {}, payload_length = {}, next_header = {}, hop_limit = {}, src = {}, dst = {}\n",
          header.version, header.traffic_class1, header.traffic_class2, header.flow_label,
          byteswap(header.payload_length), header.next_header, header.hop_limit,
          header.src, header.dst);

    switch (header.next_header) {
        case ip_proto::icmpv6:
            handle_icmpv6_packet(netif, span(buf.data() + sizeof(ipv6_header), buf.size() - sizeof(ipv6_header)));
            break;

        default:
            break;
    }
}

static bool drop_packet(const mac_address& dst, const mac_address& us) {
    if (dst == us)
        return false;

    // IPv6 multicast
    if (dst.v[0] == 0x33 && dst.v[1] == 0x33)
        return false;

    // broadcast
    if (dst.v[0] == 0xff && dst.v[1] == 0xff && dst.v[2] == 0xff &&
        dst.v[3] == 0xff && dst.v[4] == 0xff && dst.v[5] == 0xff) {
        return false;
    }

    return true;
}

template<typename T>
void virtio_net<T>::recv_thread() {
    print("FIXME - virtio_net::recv_thread\n"); // FIXME

    // FIXME - better to have pool of read buffers? And do more work in interrupt handler?

    // FIXME - die gracefully if device ever goes away

    auto buf_size = sizeof(virtio_net_hdr) + sizeof(ethernet_header) + mtu;
    auto buf = (uint8_t*)malloc(buf_size);
    if (!buf) {
        print("malloc failed\n");
        return;
    }

    while (true) {
        uint64_t buf_read_bytes; //, written_bytes;

        auto status = receive_queue->read(span(buf, buf_size), buf_read_bytes);

        if (!success(status)) {
            print("receive_queue->read returned {}\n", status);
            continue;
        }

        if (buf_read_bytes < sizeof(virtio_net_hdr) + sizeof(ethernet_header)) {
            print("virtio_net::recv_thread: received {} bytes, expected at least {}\n",
                  buf_read_bytes, sizeof(virtio_net_hdr) + sizeof(ethernet_header));
            continue;
        }

        print("buf_read_bytes = {}\n", buf_read_bytes);

        auto& hdr = *(virtio_net_hdr*)buf;

        print("hdr: flags = {}, gso_type = {}, hdr_len = {}, gso_size = {}, csum_start = {}, csum_offset = {}, num_buffers = {}\n",
              hdr.flags, hdr.gso_type, hdr.hdr_len, hdr.gso_size, hdr.csum_start,
              hdr.csum_offset, hdr.num_buffers);

        auto& eth = *(ethernet_header*)(buf + sizeof(virtio_net_hdr));

        print("ethernet: dst_mac = {}, src_mac = {}, type = {}\n",
              eth.dst_mac, eth.src_mac, eth.type);

        if (drop_packet(eth.dst_mac, mac))
            continue;

        auto data = span(buf + sizeof(virtio_net_hdr) + sizeof(ethernet_header),
                         buf_read_bytes - sizeof(virtio_net_hdr) - sizeof(ethernet_header));

        if (eth.type == ethertype::ipv6)
            handle_ipv6_packet(*this, data);
    }
}
