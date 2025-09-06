#include <stdint.h>
#include <string_view>
#include <span>
#include <concepts>
#include <system_error>
#include <charconv>
#include <optional>
#include <loaderblock.h>
#include "devicetree.h"

using namespace std;

static const uint64_t PAGE_SIZE = 4096;
static const unsigned int PAGE_SHIFT = 12;

extern uint8_t payload_data;
extern const uint64_t payload_size;
extern uint32_t image_size;

static const uint64_t PERIPHERAL_BASE = 0xfe000000;
static const uint64_t GPFSEL0 = PERIPHERAL_BASE + 0x200000;
static const uint64_t GPSET0 = PERIPHERAL_BASE + 0x20001c;
static const uint64_t GPCLR0 = PERIPHERAL_BASE + 0x200028;
static const uint64_t GPPUPPDN0 = PERIPHERAL_BASE + 0x2000e4;

static const unsigned int GPIO_MAX_PIN = 53;
static const unsigned int GPIO_FUNCTION_ALT5 = 2;

static const unsigned int Pull_None = 0;

static const uint64_t AUX_BASE = PERIPHERAL_BASE + 0x215000;
static const uint64_t AUX_ENABLES = AUX_BASE + 4;
static const uint64_t AUX_MU_IO_REG = AUX_BASE + 64;
static const uint64_t AUX_MU_IER_REG = AUX_BASE + 68;
static const uint64_t AUX_MU_IIR_REG = AUX_BASE + 72;
static const uint64_t AUX_MU_LCR_REG = AUX_BASE + 76;
static const uint64_t AUX_MU_MCR_REG = AUX_BASE + 80;
static const uint64_t AUX_MU_LSR_REG = AUX_BASE + 84;
static const uint64_t AUX_MU_CNTL_REG = AUX_BASE + 96;
static const uint64_t AUX_MU_BAUD_REG = AUX_BASE + 104;
static const uint64_t AUX_UART_CLOCK = 500000000;

struct ARM64_HARDWARE_PTE {
    uint64_t valid : 1;
    uint64_t table_entry : 1;
    uint64_t AttrIndex : 3;
    uint64_t NS : 1;
    uint64_t AP : 2;
    uint64_t SH : 2;
    uint64_t AF : 1;
    uint64_t nG : 1;
    uint64_t pfn : 39;
    uint64_t DBM : 1;
    uint64_t Contig : 1;
    uint64_t PXN : 1;
    uint64_t UXN : 1;
    uint64_t software : 4;
    uint64_t imp_def : 4;
    uint64_t : 1;
} __attribute((packed));

static_assert(sizeof(ARM64_HARDWARE_PTE) == 8);

union ARM64_TCR_EL1 {
    struct {
        uint64_t T0SZ : 6;
        uint64_t : 1;
        uint64_t EPD0 : 1;
        uint64_t IRGN0 : 2;
        uint64_t ORGN0 : 2;
        uint64_t SH0 : 2;
        uint64_t TG0 : 2;
        uint64_t T1SZ : 6;
        uint64_t A1 : 1;
        uint64_t EPD1 : 1;
        uint64_t IRGN1 : 2;
        uint64_t ORGN1 : 2;
        uint64_t SH1 : 2;
        uint64_t TG1 : 2;
        uint64_t IPS : 3;
        uint64_t : 1;
        uint64_t AS : 1;
        uint64_t TBI0 : 1;
        uint64_t TBI1 : 1;
        uint64_t HA : 1;
        uint64_t HD : 1;
        uint64_t HPD0 : 1;
        uint64_t HPD1 : 1;
        uint64_t HWU059 : 1;
        uint64_t HWU060 : 1;
        uint64_t HWU061 : 1;
        uint64_t HWU062 : 1;
        uint64_t HWU159 : 1;
        uint64_t HWU160 : 1;
        uint64_t HWU161 : 1;
        uint64_t HWU162 : 1;
        uint64_t TBID0 : 1;
        uint64_t TBID1 : 1;
        uint64_t NFD0 : 1;
        uint64_t NFD1 : 1;
        uint64_t E0PD0 : 1;
        uint64_t E0PD1 : 1;
        uint64_t TCMA0 : 1;
        uint64_t TCMA1 : 1;
        uint64_t DS : 1;
        uint64_t MTX0 : 1;
        uint64_t MTX1 : 1;
        uint64_t : 2;
    };
    uint64_t value;
} __attribute((packed));

static_assert(sizeof(ARM64_TCR_EL1) == 8);

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)

#define MAX_MAP_ENTRIES 256

static map_entry mem_entries[MAX_MAP_ENTRIES] __attribute__((section (".data")));
static unsigned int mem_entries_count __attribute__((section (".data"))) = 0;

// mem.S
extern "C" uint8_t get_current_el();
extern "C" void el2_to_el1();
extern "C" void el3_to_el1();
extern "C" uint64_t tcr_el1_load();
extern "C" void tcr_el1_store(uint64_t tcr_el1);
extern "C" void enable_paging(uint64_t ttbr0, uint64_t ttbr1);

extern "C"
size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

extern "C"
void* memmove(void* dest, const void* src, size_t n) {
    auto orig_dest = dest;

    if (dest <= src) {
        while (n > 0) {
            *(uint8_t*)dest = *(uint8_t*)src;

            dest = (uint8_t*)dest + 1;
            src = (uint8_t*)src + 1;

            n--;
        }
    } else {
        dest = (uint8_t*)dest + n - 1;
        src = (uint8_t*)src + n - 1;

        while (n > 0) {
            *(uint8_t*)dest = *(uint8_t*)src;

            dest = (uint8_t*)dest - 1;
            src = (uint8_t*)src - 1;

            n--;
        }
    }

    return orig_dest;
}

extern "C"
int memcmp(const void* s1, const void* s2, size_t n) {
    // FIXME - not completely accurate, as 1 or -1 is messed up by endianness

    // FIXME - can only do one byte at a time because we're in aligned mode?

    while (n > 0) {
        uint8_t c1 = *(uint8_t*)s1;
        uint8_t c2 = *(uint8_t*)s2;

        if (c1 != c2)
            return c1 > c2 ? 1 : -1;

        s1 = (uint8_t*)s1 + 1;
        s2 = (uint8_t*)s2 + 1;
        n--;
    }

    return 0;
}

extern "C"
void* memcpy(void* dest, const void* src, size_t n) {
    void* orig_dest = dest;

    while (n >= sizeof(uint8_t)) {
        *(uint8_t*)dest = *(uint8_t*)src;

        dest = (uint8_t*)dest + sizeof(uint8_t);
        src = (uint8_t*)src + sizeof(uint8_t);

        n -= sizeof(uint8_t);
    }

    return orig_dest;
}

extern "C"
void* memset(void* s, int c, size_t n) {
    void* orig_s = s;

    while (n > 0) {
        *(uint8_t*)s = (uint8_t)c;

        s = (uint8_t*)s + 1;
        n--;
    }

    return orig_s;
}

static void mmio_write(uint64_t reg, unsigned int val) {
    *(volatile unsigned int *)reg = val;
}

static unsigned int mmio_read(uint64_t reg) {
    return *(volatile unsigned int *)reg;
}

static void gpio_call(unsigned int pin_number, unsigned int value, unsigned int base,
                      unsigned int field_size, unsigned int field_max) {
    unsigned int field_mask = (1 << field_size) - 1;

    if (pin_number > field_max)
        return;

    if (value > field_mask)
        return;

    unsigned int num_fields = 32 / field_size;
    unsigned int reg = base + ((pin_number / num_fields) * 4);
    unsigned int shift = (pin_number % num_fields) * field_size;

    unsigned int curval = mmio_read(reg);
    curval &= ~(field_mask << shift);
    curval |= value << shift;
    mmio_write(reg, curval);
}

static void gpio_pull(unsigned int pin_number, unsigned int value) {
    return gpio_call(pin_number, value, GPPUPPDN0, 2, GPIO_MAX_PIN);
}

static void gpio_function(unsigned int pin_number, unsigned int value) {
    return gpio_call(pin_number, value, GPFSEL0, 3, GPIO_MAX_PIN);
}

static void gpio_use_as_alt_5(unsigned int pin_number) {
    gpio_pull(pin_number, Pull_None);
    gpio_function(pin_number, GPIO_FUNCTION_ALT5);
}

static void uart_init() {
    mmio_write(AUX_ENABLES, 1); // enable UART1
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_CNTL_REG, 0);
    mmio_write(AUX_MU_LCR_REG, 3); // 8 bits
    mmio_write(AUX_MU_MCR_REG, 0);
    mmio_write(AUX_MU_IER_REG, 0);
    mmio_write(AUX_MU_IIR_REG, 0xc6); // disable interrupts
    mmio_write(AUX_MU_BAUD_REG, AUX_MU_BAUD(115200));
    gpio_use_as_alt_5(14);
    gpio_use_as_alt_5(15);
    mmio_write(AUX_MU_CNTL_REG, 3); // enable RX/TX
}

static unsigned int uart_is_write_byte_ready() {
    return mmio_read(AUX_MU_LSR_REG) & 0x20;
}

static void uart_write_blocking(char ch) {
    while (!uart_is_write_byte_ready()) {
    }

    mmio_write(AUX_MU_IO_REG, (unsigned int)ch);
}

static void print(string_view sv) {
    for (auto c : sv) {
       if (c == '\n')
           uart_write_blocking('\r');

       uart_write_blocking(c);
    }
}

static void print_hex(integral auto i) {
    char buf[20];

    if (auto [ptr, ec] = to_chars(buf, buf + sizeof(buf), i, 16); ec == errc())
        print(string_view(buf, (size_t)(ptr - buf))); // FIXME
}

static bool add_map_entry(uint64_t address, uint64_t size, enum mem_type type) {
    if (mem_entries_count >= MAX_MAP_ENTRIES) {
        print("maximum number of mem entries exceeded\n");
        return false;
    }

    auto& m = mem_entries[mem_entries_count];

    m.addr = address;
    m.len = size;
    m.type = type;

    mem_entries_count++;

    return true;
}

static bool parse_dtb_memory(uint8_t* dt, char* strings, uint32_t address_cells,
                             uint32_t size_cells) {
    if (address_cells == 0 || address_cells > 2) {
        print("invalid address_cells value ");
        print_hex(address_cells);
        print("\n");
        return false;
    }

    if (size_cells == 0 || size_cells > 2) {
        print("invalid size_cells value ");
        print_hex(size_cells);
        print("\n");
        return false;
    }

    while (true) {
        auto type = (enum fdt_type)__builtin_bswap32(*(uint32_t*)dt);

        dt += sizeof(uint32_t);

        if (type == fdt_type::PROP) {
            auto& p = *(dtb_prop*)dt;

            string_view name = strings + __builtin_bswap32(p.nameoff);

            dt += sizeof(dtb_prop);

            static const char reg[] = "reg";

            if (name.size() == sizeof(reg) - 1 && !memcmp(name.data(), reg, sizeof(reg) - 1)) {
                auto len = __builtin_bswap32(p.len);
                span sp((uint32_t*)dt, len / sizeof(uint32_t));

                for (size_t i = 0; i < sp.size(); ) {
                    uint64_t address, size;

                    if (address_cells == 2) {
                        address = __builtin_bswap32(sp[i]);
                        address <<= 32;
                        address |= __builtin_bswap32(sp[i+1]);
                        i += 2;
                    } else {
                        address = __builtin_bswap32(sp[i]);
                        i++;
                    }

                    if (size_cells == 2) {
                        size = __builtin_bswap32(sp[i]);
                        size <<= 32;
                        size |= __builtin_bswap32(sp[i+1]);
                        i += 2;
                    } else {
                        size = __builtin_bswap32(sp[i]);
                        i++;
                    }

                    if (!add_map_entry(address, size, mem_type::free))
                        return false;
                }
            }

            dt += __builtin_bswap32(p.len);

            if ((uintptr_t)dt % sizeof(uint32_t))
                dt += sizeof(uint32_t) - ((uintptr_t)dt % sizeof(uint32_t));
        } else
            break;
    }

    return true;
}

static bool parse_dtb(uint8_t* dt, char* strings) {
    unsigned int depth = 0;
    uint32_t address_cells = 0, size_cells = 0;

    while (true) {
        auto type = (enum fdt_type)__builtin_bswap32(*(uint32_t*)dt);

        dt += sizeof(uint32_t);

        if (type == fdt_type::END)
            break;

        if (type == fdt_type::END_NODE && depth == 1)
            break;

        switch (type) {
            case fdt_type::BEGIN_NODE: {
                string_view name = (const char*)dt;

                print("BEGIN_NODE \"");
                print(name);
                print("\"\n");

                dt += name.length() + 1;

                if ((uintptr_t)dt % sizeof(uint32_t))
                    dt += sizeof(uint32_t) - ((uintptr_t)dt % sizeof(uint32_t));

                static const char pref[] = "memory@";

                if (name.size() >= sizeof(pref) - 1 && !memcmp(name.data(), pref, sizeof(pref) - 1)) {
                    if (!parse_dtb_memory(dt, strings, address_cells, size_cells))
                        return false;
                }

                depth++;

                break;
            }

            case fdt_type::END_NODE:
                print("END_NODE\n");
                depth--;
                break;

            case fdt_type::PROP: {
                auto& p = *(dtb_prop*)dt;

                string_view name = strings + __builtin_bswap32(p.nameoff);

                dt += sizeof(dtb_prop);

                static const char address_cells_str[] = "#address-cells";
                static const char size_cells_str[] = "#size-cells";

                if (depth == 1 && __builtin_bswap32(p.len) == sizeof(uint32_t)) {
                    if (name.size() >= sizeof(address_cells_str) - 1 && !memcmp(name.data(), address_cells_str, sizeof(address_cells_str) - 1))
                        address_cells = __builtin_bswap32(*(uint32_t*)dt);
                    else if (name.size() >= sizeof(size_cells_str) - 1 && !memcmp(name.data(), size_cells_str, sizeof(size_cells_str) - 1))
                        size_cells = __builtin_bswap32(*(uint32_t*)dt);
                }

                print("PROP \"");
                print(name);
                print("\"\n");

                dt += __builtin_bswap32(p.len);

                if ((uintptr_t)dt % sizeof(uint32_t))
                    dt += sizeof(uint32_t) - ((uintptr_t)dt % sizeof(uint32_t));

                break;
            }

            case fdt_type::NOP:
                break;

            case fdt_type::END:
                unreachable();

            default:
                print("unhandled FDT type ");
                print_hex((unsigned int)type);
                print("\n");
                return false;
        }

        // FIXME
    }

    return true;
}

static weak_ordering cmp_mem_type(enum mem_type m1, enum mem_type m2) {
    if ((unsigned int)m1 == (unsigned int)m2)
        return weak_ordering::equivalent;

    if (m1 == mem_type::free)
        return weak_ordering::less;

    if (m2 == mem_type::free)
        return weak_ordering::greater;

    if (m1 == mem_type::reserved)
        return weak_ordering::greater;

    if (m2 == mem_type::reserved)
        return weak_ordering::less;

    return weak_ordering::equivalent;
}

static void add_new_mem_entry(unsigned int pos) {
    for (int i = (int)mem_entries_count; i >= (int)pos; i--) {
        mem_entries[i + 1] = mem_entries[i];
    }
    mem_entries_count++;
}

static void remove_mem_entry(unsigned int pos) {
    for (unsigned int i = pos; i < mem_entries_count; i++) {
        mem_entries[i] = mem_entries[i + 1];
    }
    mem_entries_count--;
}

static void sanitize_mem_entries() {
    span mem(mem_entries, mem_entries_count);

    // align to pages
    for (auto& m : mem) {
        if (m.addr & (PAGE_SIZE - 1)) {
            uint64_t new_addr = m.addr & ~(PAGE_SIZE - 1);

            m.len += m.addr - new_addr;
            m.addr = new_addr;
        }

        if (m.len & (PAGE_SIZE - 1))
            m.len = (m.len & ~(PAGE_SIZE - 1)) + PAGE_SIZE;
    }

    // remove any with size 0
    for (unsigned int i = 0; i < mem_entries_count; i++) {
        if (mem_entries[i].len == 0) {
            swap(mem_entries[i], mem_entries[mem_entries_count - 1]);
            mem_entries_count--;
        }
    }

    // sort
    while (true) {
        bool changed = false;

        for (unsigned i = 0; i < mem_entries_count - 1; i++) {
            auto& a = mem_entries[i];
            auto& b = mem_entries[i + 1];

            if (a.addr > b.addr) {
                swap(a, b);
                changed = true;
                i--;
            }
        }

        if (!changed)
            break;
    }

    // handle overlaps
    for (unsigned int i = 0; i < mem_entries_count - 1; i++) {
        auto& a = mem_entries[i];
        auto& b = mem_entries[i + 1];

        if (b.addr >= a.addr && b.addr < a.addr + a.len) {
            auto cmp = cmp_mem_type(a.type, b.type);

            if (b.addr + b.len < a.addr + a.len) {
                // second entirely within first

                if (cmp == weak_ordering::less) {
                    uint64_t a_end = a.addr + a.len;

                    a.len = b.addr - a.addr;

                    // FIXME - check for overflow
                    add_new_mem_entry(i + 2);

                    auto& c = mem_entries[i + 2];
                    c.addr = b.addr + b.len;
                    c.len = a_end - c.addr;
                    c.type = a.type;
                } else
                    b.len = 0;
            } else {
                if (cmp == weak_ordering::less)
                    a.len = b.addr - a.addr;
                else {
                    b.len -= a.addr + a.len - b.addr;
                    b.addr = a.addr + a.len;
                }
            }

            auto new_i = i;

            if (b.len == 0) {
                remove_mem_entry(i + 1);
                new_i--;
            }

            if (a.len == 0)
                remove_mem_entry(i);

            i = new_i;
        }
    }

    // add any missing
    for (unsigned int i = 0; i < mem_entries_count - 1; i++) {
        auto& a = mem_entries[i];
        auto& b = mem_entries[i + 1];

        if (b.addr > a.addr + a.len) {
            uint64_t new_address = a.addr + a.len;
            uint64_t new_length = b.addr - a.addr - a.len;

            add_new_mem_entry(i + 1);
            mem_entries[i + 1].addr = new_address;
            mem_entries[i + 1].len = new_length;
            mem_entries[i + 1].type = mem_type::reserved;

            i--;
        }
    }

    // combine adjacent
    for (unsigned int i = 0; i < mem_entries_count - 1; i++) {
        auto& a = mem_entries[i];
        auto& b = mem_entries[i + 1];

        if (a.type == b.type) {
            a.len = b.addr + b.len - a.addr;
            remove_mem_entry(i + 1);
            i--;
        }
    }

    // remove bad or reserved bits at end
    while (mem_entries_count > 0 && mem_entries[mem_entries_count - 1].type == mem_type::reserved) {
        mem_entries_count--;
    }
}

static bool find_memory(uint64_t dtb, uint64_t image_base) {
    if (dtb == 0) {
        print("No DTB provided.\n");
        return false;
    }

    const auto& h = *(fdt_header*)dtb;

    if (__builtin_bswap32(h.magic) != DEVICE_TREE_MAGIC) {
        print("Device tree had invalid magic.\n");
        return false;
    }

    if (__builtin_bswap32(h.version) != DEVICE_TREE_VERSION) {
        print("Device tree is version ");
        print_hex(__builtin_bswap32(h.version));
        print(", expected ");
        print_hex(__builtin_bswap32(DEVICE_TREE_VERSION));
        print("had invalid magic.\n");
        return false;
    }

    if (!parse_dtb((uint8_t*)dtb + __builtin_bswap32(h.off_dt_struct),
                   (char*)dtb + __builtin_bswap32(h.off_dt_strings))) {
        return false;
    }

    auto* rsvmap = (fdt_reserve_entry*)(dtb + __builtin_bswap32(h.off_mem_rsvmap));

    while (true) {
        if (rsvmap->address == 0 && rsvmap->size == 0)
            break;

        add_map_entry(__builtin_bswap64(rsvmap->address),
                      __builtin_bswap64(rsvmap->size), mem_type::reserved);
        rsvmap++;
    }

    // FIXME - do we need to parse /reserved-memory?

    add_map_entry(image_base, image_size, mem_type::bootloader);

    // FIXME - add devicetree memory

    sanitize_mem_entries();

    for (unsigned int i = 0; i < mem_entries_count; i++) {
        print("mapping: addr = ");
        print_hex(mem_entries[i].addr);
        print(", len = ");
        print_hex(mem_entries[i].len);
        print(", type = ");
        print_hex((unsigned int)mem_entries[i].type);
        print("\n");
    }

    return true;
}

static optional<uint64_t> alloc_page(enum mem_type type) {
    for (unsigned int i = 0; i < mem_entries_count; i++) {
        uint64_t ret;

        auto& m = mem_entries[i];

        if (m.type != mem_type::free)
            continue;

        ret = m.addr;

        // FIXME - handle merging with next

        // extending previous entry
        if (i > 0 && mem_entries[i - 1].type == type) {
            mem_entries[i - 1].len += PAGE_SIZE;
            m.addr += PAGE_SIZE;
            m.len -= PAGE_SIZE;

            if (m.len == 0)
                remove_mem_entry(i);

            return ret;
        }

        // free block of same size
        if (m.len == PAGE_SIZE) {
            m.type = type;
            return ret;
        }

        // add new entry
        add_new_mem_entry(i + 1);

        mem_entries[i + 1].addr = ret + PAGE_SIZE;
        mem_entries[i + 1].len = m.len - PAGE_SIZE;
        mem_entries[i + 1].type = mem_type::free;

        m.len = PAGE_SIZE;
        m.type = type;

        return ret;
    }

    return nullopt;
}

static bool do_paging() {
    uint64_t level0, level1;

    if (auto ret = alloc_page(mem_type::paging_table); !ret.has_value()) {
        print("alloc_page failed\n");
        return false;
    } else
        level0 = *ret;

    if (auto ret = alloc_page(mem_type::paging_table); !ret.has_value()) {
        print("alloc_page failed\n");
        return false;
    } else
        level1 = *ret;

    auto pte0 = span((ARM64_HARDWARE_PTE*)level0, PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));
    auto pte1 = span((ARM64_HARDWARE_PTE*)level1, PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE));

    memset(pte0.data(), 0, pte0.size_bytes());
    memset(pte1.data(), 0, pte1.size_bytes());

    // FIXME - check ID_AA64MMFR0_EL1 that 4KB page size supported

    pte0[0].valid = 1;
    pte0[0].table_entry = 1;
    pte0[0].AttrIndex = 0; // FIXME?
    pte0[0].AP = 0; // no access from userspace
    pte0[0].SH = 3; // inner shareable
    pte0[0].AF = 1;
    pte0[0].pfn = level1 >> PAGE_SHIFT;

    unsigned int num_mappable_mem_entries = mem_entries_count;

    while (num_mappable_mem_entries > 0 && (mem_entries[num_mappable_mem_entries - 1].type == mem_type::free || mem_entries[num_mappable_mem_entries - 1].type == mem_type::reserved)) {
        num_mappable_mem_entries--;
    }

    const auto& last_mappable = mem_entries[num_mappable_mem_entries - 1];
    static const uint64_t SZ_1G = 0x40000000;

    uint64_t last_address = last_mappable.addr + last_mappable.len;
    auto num_gigs = last_address / SZ_1G;

    if (last_address % SZ_1G)
        num_gigs++;

    // FIXME - fails if would map more than 512GB
    if (num_gigs > PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE))
        num_gigs = PAGE_SIZE / sizeof(ARM64_HARDWARE_PTE);

    for (unsigned int i = 0; i < num_gigs; i++) {
        pte1[i].valid = 1;
        pte1[i].table_entry = 0;
        pte1[i].AttrIndex = 0; // FIXME?
        pte1[i].AP = 0; // no access from userspace
        pte1[i].SH = 3; // inner shareable
        pte1[i].AF = 1;
        pte1[i].pfn = ((uint64_t)i * SZ_1G) >> PAGE_SHIFT;
    }

    // FIXME - set MAIR_EL1

    ARM64_TCR_EL1 tcr;

    tcr.value = tcr_el1_load();

    tcr.TG0 = 3; // 64KB granule size
    tcr.T0SZ = 16; // 48-bit memory size
    tcr.T1SZ = 16; // 48-bit memory size
    tcr.HA = 0; // hardware access flag disabled

    tcr_el1_store(tcr.value);

    enable_paging(level0, 0);

    return true;
}

extern "C"
void start(uint64_t dtb, uint64_t image_base) {
    uart_init();

    auto el = get_current_el();
    if (el == 0) {
        print("currently running in EL0, bailing\n");
        goto end;
    }

    print("dtb = ");
    print_hex(dtb);
    print("\n");

    print("image_base = ");
    print_hex(image_base);
    print(", image_size = ");
    print_hex(image_size);
    print("\n");

    if (!find_memory(dtb, image_base))
        goto end;

    // FIXME - read cmdline
    // FIXME - find initrd

    // FIXME - load image

    if (el == 2)
        el2_to_el1();
    else if (el == 3)
        el3_to_el1();

    if (!do_paging())
        goto end;

    // FIXME - jump to entry point

end:
    while (true) {
    }
}
