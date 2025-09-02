#include <stdint.h>
#include <string_view>
#include <concepts>
#include <system_error>
#include <charconv>
#include <loaderblock.h>
#include "devicetree.h"

using namespace std;

extern uint8_t payload_data;
extern const uint64_t payload_size;

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

#define AUX_MU_BAUD(baud) ((AUX_UART_CLOCK/(baud*8))-1)

#define MAX_MAP_ENTRIES 256

static map_entry map_entries[MAX_MAP_ENTRIES] __attribute__((section (".data")));
static unsigned int num_map_entries __attribute__((section (".data"))) = 0;

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

static void parse_dtb_memory(uint8_t* dt) {
    // FIXME
    print("memory!\n");
}

static bool parse_dtb(uint8_t* dt, char* strings) {
    unsigned int depth = 0;

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

                if (name.size() >= sizeof(pref) - 1 && !memcmp(name.data(), pref, sizeof(pref) - 1))
                    parse_dtb_memory(dt);

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

                print("PROP \"");
                print(name);
                print("\"\n");

                dt += sizeof(dtb_prop);
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

static bool find_memory(uint64_t dtb) {
    if (dtb == 0) {
        print("No DTB provided.\n");
        return false;
    }

    const auto& h = *(fdt_header*)dtb;

    if (h.magic != DEVICE_TREE_MAGIC) {
        print("Device tree had invalid magic.\n");
        return false;
    }

    if (!parse_dtb((uint8_t*)dtb + __builtin_bswap32(h.off_dt_struct),
                   (char*)dtb + __builtin_bswap32(h.off_dt_strings)))
        return false;

    // FIXME - parse DTB
    // FIXME - parse /memory
    // FIXME - parse rsvmap
    // FIXME - parse /reserved-memory

    return true;
}

extern "C"
void start(uint64_t dtb, uint64_t image_base) {
    uart_init();

    print("dtb = ");
    print_hex(dtb);
    print("\n");

    print("image_base = ");
    print_hex(image_base);
    print("\n");

    if (!find_memory(dtb))
        goto end;

end:
    while (true) {
    }
}
