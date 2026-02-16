#include <span>
#include <atomic>
#include <string_view>

import nt;
import print;
import chardev;
import format;
import pci;
import virtio;
import list;
import module_init;
import thread;
import handle;
import object_ref;
import net;

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

template<typename T>
class virtio_net : public chardev {
public:
    virtio_net(pci_device& d);
    virtio_net(T& t);

    ~virtio_net() {
        delete &dev;
    }

    nt_status read(span<uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const uint8_t>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    void recv_thread();

    T& dev;
    mac_address mac;
    uint16_t mtu;

private:
    virt_queue* receive_queue = nullptr;
    virt_queue* transmit_queue = nullptr;
};

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

    auto& cfg = *(virtio_net_config*)(d.bar[dev.device_cfg->bar].data() + dev.device_cfg->offset);

    memcpy(&mac, cfg.mac, sizeof(mac));

    if (dev.negotiated_features & VIRTIO_NET_F_MTU)
        mtu = cfg.mtu;
    else
        mtu = DEFAULT_MTU;

    print("found virtio-net device with MAC {}, MTU {}\n", mac, mtu);

#if 0
    print("virtio_net cfg: mac = {:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}, status = {}, max_virtqueue_pairs = {}, mtu = {}, speed = {}, duplex = {}, rss_max_key_size = {}, rss_max_indirection_table_length = {}, supported_hash_types = {}, supported_tunnel_types = {}\n",
          cfg.mac[0], cfg.mac[1], cfg.mac[2], cfg.mac[3], cfg.mac[4], cfg.mac[5],
          cfg.status, cfg.max_virtqueue_pairs, cfg.mtu, cfg.speed, cfg.duplex,
          cfg.rss_max_key_size, cfg.rss_max_indirection_table_length,
          cfg.supported_hash_types, cfg.supported_tunnel_types);
#endif

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

    auto& cfg = *(virtio_net_config*)dev.config;

    memcpy(&mac, cfg.mac, sizeof(mac));

    if (dev.negotiated_features & VIRTIO_NET_F_MTU)
        mtu = cfg.mtu;
    else
        mtu = DEFAULT_MTU;

    print("found virtio-net device with MAC {}, MTU {}\n", mac, mtu);

    for (auto le = dev.virtqueues.next; le != &dev.virtqueues; le = le->next) {
        auto& vq = containing_record<virt_queue>(le);

        if (vq.num == 0)
            receive_queue = &vq;
        else if (vq.num == 1)
            transmit_queue = &vq;
    }

    add_kernel_thread([](void* ctx) {
        ((virtio_net<virtio_mmio_device>*)ctx)->recv_thread();
    }, this);
}

static atomic<uint32_t> virtio_net_num = 0;

void found_virtio_pci_net(pci_device& dev) {
    auto pv = new virtio_net<virtio_pci_device>(dev);
    if (!pv) {
        print("new failed\n");
        return;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return;

    v.dev.finalize();

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "virtnet{}", virtio_net_num.fetch_add(1));

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status))
            print("name_object returned {}\n", status);
    }
}

static bool found_virtio_mmio_net(virtio_mmio_device& dev) {
    auto pv = new virtio_net<virtio_mmio_device>(dev);
    if (!pv) {
        print("new failed\n");
        return false;
    }

    object_ref ref{*pv};

    auto& v = *pv;

    if (!v.dev.initialized)
        return false;

    v.dev.finalize();

    {
        char s[10];
        span sp(s, sizeof(s));

        sp = format(sp, "virtnet{}", virtio_net_num.fetch_add(1));

        auto status = name_object(v, string_view(s, (size_t)(sp.data() - s)));
        if (!success(status)) {
            print("name_object returned {}\n", status);
            return false;
        }
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

        if (buf_read_bytes == 0)
            continue;

        print("buf_read_bytes = {}\n", buf_read_bytes);

        if (buf_read_bytes < sizeof(virtio_net_hdr) + sizeof(ethernet_header)) {
            print("virtio_net::recv_thread: received {} bytes, expected at least {}\n",
                  buf_read_bytes, sizeof(virtio_net_hdr) + sizeof(ethernet_header));
            continue;
        }

        auto& hdr = *(virtio_net_hdr*)buf;

        print("hdr: flags = {}, gso_type = {}, hdr_len = {}, gso_size = {}, csum_start = {}, csum_offset = {}, num_buffers = {}\n",
              hdr.flags, hdr.gso_type, hdr.hdr_len, hdr.gso_size, hdr.csum_start,
              hdr.csum_offset, hdr.num_buffers);

        auto& eth = *(ethernet_header*)(buf + sizeof(virtio_net_hdr));

        print("ethernet: dst = {}, src = {}, ethertype = {:04x}\n",
              eth.dst_mac, eth.src_mac, byteswap(eth.ethertype));

        // FIXME - process
    }
}
