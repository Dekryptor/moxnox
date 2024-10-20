#include "kernel.h"

using namespace std;

struct dwarf_cu_header {
    uint32_t unit_length;
    uint16_t version;
    uint8_t unit_type;
    uint8_t address_size;
    uint32_t debug_abbrev_object;
} __attribute__((packed));

template<typename T>
concept cus_func = invocable<T, span<const uint8_t>, uint32_t> &&
requires(T t, span<const uint8_t> s, uint32_t n) { { t(s, n) } -> convertible_to<bool>; };

static void loop_through_cus(span<const uint8_t> info, cus_func auto func) {
    do {
        const auto& h = *(dwarf_cu_header*)info.data();

        if (info.size() < sizeof(dwarf_cu_header))
            return;

        if (h.unit_length >= 0xfffffff0) // FIXME - support 64-bit DWARF
            return;

        if (h.version != 5) // FIXME - support DWARF versions other than 5?
            return;

        auto cu = span<const uint8_t>((uint8_t*)&h + sizeof(dwarf_cu_header),
                                      h.unit_length - sizeof(dwarf_cu_header) + sizeof(uint32_t));

        if (!func(cu, h.debug_abbrev_object))
            return;

        info = span(info.data() + h.unit_length + sizeof(uint32_t),
                    info.size() - h.unit_length - sizeof(uint32_t));
    } while (true);
}

string_view dwarf_find_function(const kernel_image& img, uint64_t addr) {
    auto info = img.debug_info;
    if (info.empty())
        return "";

    auto abbrev = img.debug_abbrev;
    if (abbrev.empty())
        return "";

    loop_through_cus(info, [](span<const uint8_t> cu, uint32_t debug_abbrev_object) -> bool {
        // FIXME - find CU with addr in DW_AT_ranges

        return true;
    });

    // FIXME - loop through DIEs
    // FIXME - find DW_TAG_subprogram matching range
    // FIXME - return DW_AT_name and offset

    return "";
}
