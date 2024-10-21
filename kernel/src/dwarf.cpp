#include "kernel.h"

using namespace std;

using uleb128 = unsigned __int128;

struct dwarf_cu_header {
    uint32_t unit_length;
    uint16_t version;
    uint8_t unit_type;
    uint8_t address_size;
    uint32_t debug_abbrev_object;
} __attribute__((packed));

enum class DW_FORM : uleb128 {
    addr = 0x01,
    block2 = 0x03,
    block4 = 0x04,
    data2 = 0x05,
    data4 = 0x06,
    data8 = 0x07,
    string = 0x08,
    block = 0x09,
    block1 = 0x0a,
    data1 = 0x0b,
    flag = 0x0c,
    sdata = 0x0d,
    strp = 0x0e,
    udata = 0x0f,
    ref_addr = 0x10,
    ref1 = 0x11,
    ref2 = 0x12,
    ref4 = 0x13,
    ref8 = 0x14,
    ref_udata = 0x15,
    indirect = 0x16,
    sec_offset = 0x17,
    exprloc = 0x18,
    flag_present = 0x19,
    ref_sig8 = 0x20,
    strx = 0x1a,
    addrx = 0x1b,
    ref_sup4 = 0x1c,
    strp_sup = 0x1d,
    data16 = 0x1e,
    line_strp = 0x1f,
    implicit_const = 0x21,
    loclistx = 0x22,
    rnglistx = 0x23,
    ref_sup8 = 0x24,
    strx1 = 0x25,
    strx2 = 0x26,
    strx3 = 0x27,
    strx4 = 0x28,
    addrx1 = 0x29,
    addrx2 = 0x2a,
    addrx3 = 0x2b,
    addrx4 = 0x2c,
};

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

static uleb128 read_uleb128(span<const uint8_t>& sp) {
    uleb128 ret = 0;

    for (unsigned int i = 0; i < 19; i++) {
        uint8_t byte = sp[0];

        sp = sp.subspan(1);

        ret |= (byte & 0x7f) << (7 * i);

        if (!(byte & 0x80))
            break;
    }

    return ret;
}

static void skip_uleb128(span<const uint8_t>& sp) {
    while (sp[0] & 0x80) {
        sp = sp.subspan(1);
    }

    sp = sp.subspan(1);
}

static void skip_leb128(span<const uint8_t>& sp) {
    // FIXME

    sp = sp.subspan(1);
}

static bool find_abbrev_entry(uleb128 num, span<const uint8_t> abbrev,
                              uleb128& tag, bool& has_children,
                              span<const uint8_t>& att_list) {
    while (true) {
        if (abbrev.empty())
            return false;

        auto entry_num = read_uleb128(abbrev);
        if (entry_num == 0)
            return false;

        if (entry_num != num) {
            skip_uleb128(abbrev); // tag
            abbrev = abbrev.subspan(1); // has children

            do {
                auto att_num = read_uleb128(abbrev);
                auto form = read_uleb128(abbrev);

                if ((enum DW_FORM)form == DW_FORM::implicit_const)
                    skip_leb128(abbrev);

                if (att_num == 0)
                    break;
            } while (true);

            continue;
        }

        tag = read_uleb128(abbrev);

        has_children = abbrev[0] != 0;
        abbrev = abbrev.subspan(1);

        att_list = abbrev;

        return true;
    }
}

static void process_die(span<const uint8_t> die, span<const uint8_t> abbrev) {
    uleb128 tag;
    bool has_children;
    span<const uint8_t> att_list;

    auto abbrev_num = read_uleb128(die);

    if (!find_abbrev_entry(abbrev_num, abbrev, tag, has_children, att_list))
        return;

    // FIXME
}

string_view dwarf_find_function(const kernel_image& img, uint64_t addr) {
    auto info = img.debug_info;
    if (info.empty())
        return "";

    auto abbrev = img.debug_abbrev;
    if (abbrev.empty())
        return "";

    loop_through_cus(info, [&](span<const uint8_t> cu, uint32_t debug_abbrev_object) -> bool {
        // FIXME - find CU with addr in DW_AT_ranges
        process_die(cu, abbrev.subspan(debug_abbrev_object));

        return true;
    });

    // FIXME - loop through DIEs
    // FIXME - find DW_TAG_subprogram matching range
    // FIXME - return DW_AT_name and offset

    return "";
}
