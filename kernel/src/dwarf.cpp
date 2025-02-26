#include "kernel.h"

import format;

using namespace std;

using uleb128 = unsigned __int128;

struct dwarf_cu_header {
    uint32_t unit_length;
    uint16_t version;
    uint8_t unit_type;
    uint8_t address_size;
    uint32_t debug_abbrev_object;
} __attribute__((packed));

static_assert(sizeof(dwarf_cu_header) == 12);

enum class DW_FORM : uint8_t {
    unknown = 0x00,
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

enum class DW_AT : uint16_t {
    unknown = 0x00,
    sibling = 0x01,
    location = 0x02,
    name = 0x03,
    ordering = 0x09,
    subscr_data = 0x0a,
    byte_size = 0x0b,
    bit_offset = 0x0c,
    bit_size = 0x0d,
    element_list = 0x0f,
    stmt_list = 0x10,
    low_pc = 0x11,
    high_pc = 0x12,
    language = 0x13,
    member = 0x14,
    discr = 0x15,
    discr_value = 0x16,
    visibility = 0x17,
    import_ = 0x18,
    string_length = 0x19,
    common_reference = 0x1a,
    comp_dir = 0x1b,
    const_value = 0x1c,
    containing_type = 0x1d,
    default_value = 0x1e,
    inline_ = 0x20,
    is_optional = 0x21,
    lower_bound = 0x22,
    producer = 0x25,
    prototyped = 0x27,
    return_addr = 0x2a,
    start_scope = 0x2c,
    bit_stride = 0x2e,
    upper_bound = 0x2f,
    abstract_origin = 0x31,
    accessibility = 0x32,
    address_class = 0x33,
    artificial = 0x34,
    base_types = 0x35,
    calling_convention = 0x36,
    count = 0x37,
    data_member_location = 0x38,
    decl_column = 0x39,
    decl_file = 0x3a,
    decl_line = 0x3b,
    declaration = 0x3c,
    discr_list = 0x3d,
    encoding = 0x3e,
    external = 0x3f,
    frame_base = 0x40,
    friend_ = 0x41,
    identifier_case = 0x42,
    macro_info = 0x43,
    namelist_item = 0x44,
    priority = 0x45,
    segment = 0x46,
    specification = 0x47,
    static_link = 0x48,
    type = 0x49,
    use_location = 0x4a,
    variable_parameter = 0x4b,
    virtuality = 0x4c,
    vtable_elem_location = 0x4d,
    allocated = 0x4e,
    associated = 0x4f,
    data_location = 0x50,
    byte_stride = 0x51,
    entry_pc = 0x52,
    use_UTF8 = 0x53,
    extension = 0x54,
    ranges = 0x55,
    trampoline = 0x56,
    call_column = 0x57,
    call_file = 0x58,
    call_line = 0x59,
    description = 0x5a,
    binary_scale = 0x5b,
    decimal_scale = 0x5c,
    small = 0x5d,
    decimal_sign = 0x5e,
    digit_count = 0x5f,
    picture_string = 0x60,
    mutable_ = 0x61,
    threads_scaled = 0x62,
    explicit_ = 0x63,
    object_pointer = 0x64,
    endianity = 0x65,
    elemental = 0x66,
    pure = 0x67,
    recursive = 0x68,
    signature = 0x69,
    main_subprogram = 0x6a,
    data_bit_offset = 0x6b,
    const_expr = 0x6c,
    enum_class = 0x6d,
    linkage_name = 0x6e,
    string_length_bit_size = 0x6f,
    string_length_byte_size = 0x70,
    rank = 0x71,
    str_offsets_base = 0x72,
    addr_base = 0x73,
    rnglists_base = 0x74,
    dwo_name = 0x76,
    reference = 0x77,
    rvalue_reference = 0x78,
    macros = 0x79,
    call_all_calls = 0x7a,
    call_all_source_calls = 0x7b,
    call_all_tail_calls = 0x7c,
    call_return_pc = 0x7d,
    call_value = 0x7e,
    call_origin = 0x7f,
    call_parameter = 0x80,
    call_pc = 0x81,
    call_tail_call = 0x82,
    call_target = 0x83,
    call_target_clobbered = 0x84,
    call_data_location = 0x85,
    call_data_value = 0x86,
    noreturn = 0x87,
    alignment = 0x88,
    export_symbols = 0x89,
    deleted = 0x8a,
    defaulted = 0x8b,
    loclists_base = 0x8c,
};

enum class DW_TAG : uint8_t {
    padding = 0x00,
    array_type = 0x01,
    class_type = 0x02,
    entry_point = 0x03,
    enumeration_type = 0x04,
    formal_parameter = 0x05,
    imported_declaration = 0x08,
    label = 0x0a,
    lexical_block = 0x0b,
    member = 0x0d,
    pointer_type = 0x0f,
    reference_type = 0x10,
    compile_unit = 0x11,
    string_type = 0x12,
    structure_type = 0x13,
    subroutine_type = 0x15,
    typedef_ = 0x16,
    union_type = 0x17,
    unspecified_parameters = 0x18,
    variant = 0x19,
    common_block = 0x1a,
    common_inclusion = 0x1b,
    inheritance = 0x1c,
    inlined_subroutine = 0x1d,
    module_ = 0x1e,
    ptr_to_member_type = 0x1f,
    set_type = 0x20,
    subrange_type = 0x21,
    with_stmt = 0x22,
    access_declaration = 0x23,
    base_type = 0x24,
    catch_block = 0x25,
    const_type = 0x26,
    constant = 0x27,
    enumerator = 0x28,
    file_type = 0x29,
    friend_ = 0x2a,
    namelist = 0x2b,
    namelist_item = 0x2c,
    packed_type = 0x2d,
    subprogram = 0x2e,
    template_type_param = 0x2f,
    template_value_param = 0x30,
    thrown_type = 0x31,
    try_block = 0x32,
    variant_part = 0x33,
    variable = 0x34,
    volatile_type = 0x35,
    dwarf_procedure = 0x36,
    restrict_type = 0x37,
    interface_type = 0x38,
    namespace_ = 0x39,
    imported_module = 0x3a,
    unspecified_type = 0x3b,
    partial_unit = 0x3c,
    imported_unit = 0x3d,
    condition = 0x3f,
    shared_type = 0x40,
    type_unit = 0x41,
    rvalue_reference_type = 0x42,
    template_alias = 0x43,
    coarray_type = 0x44,
    generic_subrange = 0x45,
    dynamic_type = 0x46,
    atomic_type = 0x47,
    call_site = 0x48,
    call_site_parameter = 0x49,
    skeleton_unit = 0x4a,
    immutable_type = 0x4b,
};

enum class DW_RLE : uint8_t {
    end_of_list = 0x00,
    base_addressx = 0x01,
    startx_endx = 0x02,
    startx_length = 0x03,
    offset_pair = 0x04,
    base_address = 0x05,
    start_end = 0x06,
    start_length = 0x07
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

static void skip_leb128(span<const uint8_t>& sp) {
    while (sp[0] & 0x80) {
        sp = sp.subspan(1);
    }

    sp = sp.subspan(1);
}

template<typename T>
requires is_enum_v<T>
static T read_dw_enum(span<const uint8_t>& sp) {
    auto val = read_uleb128(sp);

    if (val >= (1 << sizeof(T) * 8))
        val = 0;

    return static_cast<T>(val);
}

static bool skip_die(span<const uint8_t>& die, span<const uint8_t> att_list) {
    while (true) {
        auto att_num = read_dw_enum<enum DW_AT>(att_list);
        auto form = read_dw_enum<enum DW_FORM>(att_list);

        if (att_num == (enum DW_AT)0)
            break;

        switch (form) {
            case DW_FORM::addr:
                die = die.subspan(sizeof(uintptr_t));
            break;

            case DW_FORM::block2:
                die = die.subspan(sizeof(uint16_t) + *(uint16_t*)die.data());
            break;

            case DW_FORM::block4:
                die = die.subspan(sizeof(uint32_t) + *(uint32_t*)die.data());
            break;

            case DW_FORM::data2:
            case DW_FORM::ref2:
            case DW_FORM::strx2:
            case DW_FORM::addrx2:
                die = die.subspan(2);
            break;

            case DW_FORM::strx3:
            case DW_FORM::addrx3:
                die = die.subspan(3);
            break;

            case DW_FORM::data4:
            case DW_FORM::ref4:
            case DW_FORM::strx4:
            case DW_FORM::addrx4:
                die = die.subspan(4);
            break;

            case DW_FORM::data8:
            case DW_FORM::ref8:
                die = die.subspan(8);
            break;

            case DW_FORM::string: {
                string_view sv((char*)die.data(), die.size());

                if (auto nul = sv.find_first_of('\0'); nul != string_view::npos)
                    die = die.subspan(nul + 1);
                else
                    die = span<const uint8_t>();

                break;
            }

            case DW_FORM::block:
            case DW_FORM::exprloc: {
                auto len = read_uleb128(die);
                die = die.subspan((size_t)len);
                break;
            }

            case DW_FORM::block1:
                die = die.subspan(1 + die[0]);
            break;

            case DW_FORM::data1:
            case DW_FORM::ref1:
            case DW_FORM::strx1:
            case DW_FORM::addrx1:
                die = die.subspan(1);
            break;

            case DW_FORM::flag:
                print("FIXME - skipping because of DW_FORM_flag\n"); // FIXME
                return false;

            case DW_FORM::sdata:
            case DW_FORM::udata:
            case DW_FORM::strx:
            case DW_FORM::addrx:
            case DW_FORM::rnglistx:
                skip_leb128(die);
            break;

            case DW_FORM::strp:
            case DW_FORM::strp_sup:
            case DW_FORM::line_strp:
            case DW_FORM::sec_offset:
                die = die.subspan(4);
            break;

            case DW_FORM::ref_addr:
                print("FIXME - skipping because of DW_FORM_ref_addr\n"); // FIXME
                return false;

            case DW_FORM::ref_udata:
                print("FIXME - skipping because of DW_FORM_ref_udata\n"); // FIXME
                return false;

            case DW_FORM::indirect:
                print("FIXME - skipping because of DW_FORM_indirect\n"); // FIXME
                return false;

            case DW_FORM::flag_present:
            break;

            case DW_FORM::ref_sig8:
                print("FIXME - skipping because of DW_FORM_ref_sig8\n"); // FIXME
                return false;


            case DW_FORM::ref_sup4:
                print("FIXME - skipping because of DW_FORM_ref_sup4\n"); // FIXME
                return false;

            case DW_FORM::data16:
                die = die.subspan(16);
            break;

            case DW_FORM::implicit_const:
            break;

            case DW_FORM::loclistx:
                print("FIXME - skipping because of DW_FORM_loclistx\n"); // FIXME
                return false;

            case DW_FORM::ref_sup8:
                print("FIXME - skipping because of DW_FORM_ref_sup8\n"); // FIXME
                return false;

            default:
                print("skipping DIE because of unknown DW_FORM value {}\n",
                      (unsigned int)form);
                return false;
        }

        if (form == DW_FORM::implicit_const)
            skip_leb128(att_list);
    }

    return true;
}

static span<const uint8_t*> load_abbrev_entries(span<const uint8_t> abbrev) {
    auto orig_abbrev = abbrev;
    unsigned int count = 0;

    while (true) {
        if (abbrev.empty())
            break;

        auto entry_num = read_uleb128(abbrev);
        if (entry_num == 0)
            break;

        skip_leb128(abbrev); // tag
        abbrev = abbrev.subspan(1); // has children

        do {
            auto att_num = read_dw_enum<enum DW_AT>(abbrev);
            auto form = read_dw_enum<enum DW_FORM>(abbrev);

            if (form == DW_FORM::implicit_const)
                skip_leb128(abbrev);

            if (att_num == (enum DW_AT)0)
                break;
        } while (true);

        count++;
    }

    if (count == 0)
        return span<const uint8_t*>();

    auto arr = (const uint8_t**)malloc(sizeof(const uint8_t*) * (count + 1));
    if (!arr)
        return span<const uint8_t*>();

    abbrev = orig_abbrev;

    for (unsigned int num = 0; num < count; num++) {
        skip_leb128(abbrev); // entry num

        arr[num] = abbrev.data();

        skip_leb128(abbrev); // tag
        abbrev = abbrev.subspan(1); // has children

        do {
            auto att_num = read_dw_enum<enum DW_AT>(abbrev);
            auto form = read_dw_enum<enum DW_FORM>(abbrev);

            if (form == DW_FORM::implicit_const)
                skip_leb128(abbrev);

            if (att_num == (enum DW_AT)0)
                break;
        } while (true);
    }

    arr[count] = abbrev.data() + abbrev.size();

    return span(arr, count + 1);
}

template<typename T>
concept die_func = invocable<T, enum DW_TAG, bool, span<const uint8_t>, span<const uint8_t>, unsigned int, uint32_t> &&
requires(T t, enum DW_TAG tag, bool has_children, span<const uint8_t> die, span<const uint8_t> att_list, unsigned int depth, uint32_t offset) { { t(tag, has_children, die, att_list, depth, offset) } -> convertible_to<bool>; };

static void loop_through_dies(span<const uint8_t> cu, span<const uint8_t*> abbrev_list,
                              die_func auto func) {
    unsigned int depth = 0;

    auto orig_cu = cu;

    while (true) {
        enum DW_TAG tag;
        bool has_children;
        auto offset = (uint32_t)(cu.data() - orig_cu.data() + sizeof(dwarf_cu_header));

        auto abbrev_num = read_uleb128(cu);

        if (abbrev_num == 0) {
            if (depth == 0)
                break;

            depth--;
            continue;
        }

        if (abbrev_num > abbrev_list.size())
            break;

        auto entry = span(abbrev_list[(size_t)abbrev_num - 1],
                          abbrev_list[(size_t)abbrev_num] - abbrev_list[(size_t)abbrev_num - 1]);

        tag = read_dw_enum<enum DW_TAG>(entry);

        has_children = entry[0] != 0;
        entry = entry.subspan(1);

        if (!func(tag, has_children, cu, entry, depth, offset))
            break;

        if (has_children)
            depth++;

        if (!skip_die(cu, entry))
            break;
    }
}

static void find_die_by_reference(span<const uint8_t> cu, uint32_t reference, span<const uint8_t*> abbrev_list,
                                  invocable<enum DW_TAG, span<const uint8_t>, span<const uint8_t>> auto func) {
    cu = span(cu.data() + reference - sizeof(dwarf_cu_header),
              cu.size() - reference + sizeof(dwarf_cu_header));

    auto abbrev_num = read_uleb128(cu);

    if (abbrev_num == 0)
        return;

    if (abbrev_num >= abbrev_list.size())
        return;

    auto entry = span(abbrev_list[(size_t)abbrev_num - 1],
                      abbrev_list[(size_t)abbrev_num] - abbrev_list[(size_t)abbrev_num - 1]);

    auto tag = read_dw_enum<enum DW_TAG>(entry);

    entry = entry.subspan(1); // has_children

    func(tag, cu, entry);
}

template<typename T>
concept att_func = invocable<T, enum DW_AT, enum DW_FORM, span<const uint8_t>> &&
requires(T t, enum DW_AT att_num, enum DW_FORM form, span<const uint8_t> data) { { t(att_num, form, data) } -> convertible_to<bool>; };

static void loop_through_atts(span<const uint8_t>& die, span<const uint8_t> att_list, att_func auto func) {
    while (true) {
        auto att_num = read_dw_enum<enum DW_AT>(att_list);
        auto form = read_dw_enum<enum DW_FORM>(att_list);

        if (att_num == (enum DW_AT)0)
            break;

        span<const uint8_t> data;
        switch (form) {
            case DW_FORM::addr:
                data = span(die.data(), sizeof(uintptr_t));
                die = die.subspan(sizeof(uintptr_t));
            break;

            case DW_FORM::block2: {
                auto len = *(uint16_t*)die.data();

                data = span(die.data() + sizeof(uint16_t), len);
                die = die.subspan(sizeof(uint16_t) + len);
                break;
            }

            case DW_FORM::block4: {
                auto len = *(uint32_t*)die.data();

                data = span(die.data() + sizeof(uint32_t), len);
                die = die.subspan(sizeof(uint32_t) + len);
                break;
            }

            case DW_FORM::data2:
            case DW_FORM::ref2:
            case DW_FORM::strx2: // FIXME
            case DW_FORM::addrx2: // FIXME
                data = span(die.data(), 2);
                die = die.subspan(2);
            break;

            case DW_FORM::strx3: // FIXME
            case DW_FORM::addrx3: // FIXME
                data = span(die.data(), 2);
                die = die.subspan(2);
            break;

            case DW_FORM::data4:
            case DW_FORM::ref4:
            case DW_FORM::strx4: // FIXME
            case DW_FORM::addrx4: // FIXME
                data = span(die.data(), 4);
                die = die.subspan(4);
            break;

            case DW_FORM::data8:
            case DW_FORM::ref8:
                data = span(die.data(), 8);
                die = die.subspan(8);
            break;

            case DW_FORM::string: {
                string_view sv((char*)die.data(), die.size());

                if (auto nul = sv.find_first_of('\0'); nul != string_view::npos) {
                    data = span(die.data(), nul);
                    die = die.subspan(nul + 1);
                } else {
                    data = die;
                    die = span<const uint8_t>();
                }

                break;
            }

            case DW_FORM::block:
            case DW_FORM::exprloc: {
                auto len = read_uleb128(die);
                data = span(die.data(), (size_t)len);
                die = die.subspan((size_t)len);
                break;
            }

            case DW_FORM::block1: {
                auto len = die[0];

                data = span(die.data() + sizeof(uint8_t), len);
                die = die.subspan(sizeof(uint8_t) + len);
                break;
            }

            case DW_FORM::data1:
            case DW_FORM::ref1:
            case DW_FORM::strx1:
            case DW_FORM::addrx1:
                data = span(die.data(), 1);
                die = die.subspan(1);
            break;

            // FIXME - DW_FORM::flag

            case DW_FORM::sdata:
            case DW_FORM::udata:
            case DW_FORM::strx: // FIXME
            case DW_FORM::addrx: // FIXME
            case DW_FORM::rnglistx: {
                auto orig_die = die;

                skip_leb128(die);
                data = span(orig_die.data(), die.data() - orig_die.data());
                break;
            }

            case DW_FORM::strp:
            case DW_FORM::strp_sup:
            case DW_FORM::line_strp:
            case DW_FORM::sec_offset:
                data = span(die.data(), 4);
                die = die.subspan(4);
            break;

            // FIXME - DW_FORM::ref_addr
            // FIXME - DW_FORM::ref_udata
            // FIXME - DW_FORM::indirect

            case DW_FORM::flag_present:
            break;

            // FIXME - DW_FORM::ref_sig8
            // FIXME - DW_FORM::ref_sup4

            case DW_FORM::data16:
                data = span(die.data(), 16);
                die = die.subspan(16);
            break;

            case DW_FORM::implicit_const: {
                auto new_att_list = att_list;

                skip_leb128(new_att_list);

                data = span(att_list.data(), new_att_list.data() - att_list.data());
                att_list = new_att_list;
                break;
            }

            // FIXME - DW_FORM::loclistx
            // FIXME - DW_FORM::ref_sup8

            default:
                print("unhandled DWARF form {:x}\n", (unsigned int)form);
                return;
        }

        if (!func(att_num, form, data))
            break;
    }
}

static bool addr_in_range(uint64_t addr, span<const uint8_t> rnglist,
                          span<const uint8_t> debug_addr,
                          optional<uint32_t> addr_base) {
    optional<uint64_t> base_address;

    while (!rnglist.empty()) {
        auto type = (enum DW_RLE)rnglist[0];

        rnglist = rnglist.subspan(1);

        switch (type) {
            case DW_RLE::end_of_list:
                return false;

            case DW_RLE::base_addressx: {
                auto index = read_uleb128(rnglist);

                if (!addr_base.has_value()) {
                    print("DW_RLE_base_addressx but no DW_AT_addr_base value\n");
                    return false;
                }

                // FIXME - check .debug_addr header that address_size == 8
                // FIXME - check .debug_addr header than segment_selector_size == 0

                if (*addr_base + ((index + 1) * sizeof(uint64_t)) > debug_addr.size()) {
                    print("DW_RLE_base_addressx would overflow .debug_addr section\n");
                    return false;
                }

                auto list = (uint64_t*)(debug_addr.data() + *addr_base);
                base_address = list[index];

                break;
            }

            case DW_RLE::startx_endx:
                print("unhandled DWARF RLE type startx_endx\n");
                // FIXME
                return false;

            case DW_RLE::startx_length: {
                auto index = read_uleb128(rnglist);
                auto length = read_uleb128(rnglist);

                if (!addr_base.has_value()) {
                    print("DW_RLE_startx_length but no DW_AT_addr_base value\n");
                    return false;
                }

                // FIXME - check .debug_addr header that address_size == 8
                // FIXME - check .debug_addr header than segment_selector_size == 0

                if (*addr_base + ((index + 1) * sizeof(uint64_t)) > debug_addr.size()) {
                    print("DW_RLE_base_addressx would overflow .debug_addr section\n");
                    return false;
                }

                auto list = (uint64_t*)(debug_addr.data() + *addr_base);
                auto start = list[index];

                if (start == 0) // FIXME - why does DWARF do this? Stuff that's optimized out?
                    break;

                if (addr >= start && addr < start + length)
                    return true;

                break;
            }

            case DW_RLE::offset_pair: {
                auto start = read_uleb128(rnglist);
                auto end = read_uleb128(rnglist);

                if (!base_address.has_value())
                    break;

                if (addr >= *base_address + start && addr <= *base_address + end)
                    return true;

                break;
            }

            case DW_RLE::base_address:
                print("unhandled DWARF RLE type base_address\n");
                // FIXME
                return false;

            case DW_RLE::start_end:
                print("unhandled DWARF RLE type start_end\n");
                // FIXME
                return false;

            case DW_RLE::start_length: {
                auto base_addr = *(uint64_t*)rnglist.data();

                rnglist = rnglist.subspan(sizeof(uint64_t));

                auto length = read_uleb128(rnglist);

                if (addr >= base_addr && addr < base_addr + length)
                    return true;

                break;
            }

            default:
                print("unhandled DWARF RLE type {:x}\n", (unsigned int)type);
                return false;
        }
    }

    return false;
}

static uint32_t find_die_parent(span<const uint8_t> cu, span<const uint8_t*> abbrev_list,
                                uint32_t offset) {
    unsigned int max_depth = 0, cur_depth;

    loop_through_dies(cu, abbrev_list, [&](enum DW_TAG, bool, span<const uint8_t>,
                                           span<const uint8_t>, unsigned int depth,
                                           uint32_t this_offset) -> bool {
        max_depth = max(max_depth, depth);

        if (this_offset > offset)
            return false;

        return true;
    });

    if (max_depth == 0)
        return 0;

    auto stack = (uint32_t*)malloc(max_depth * sizeof(uint32_t));
    if (!stack)
        return 0;

    loop_through_dies(cu, abbrev_list, [&](enum DW_TAG, bool has_children, span<const uint8_t>,
                                           span<const uint8_t>, unsigned int depth,
                                           uint32_t this_offset) -> bool {
        if (this_offset > offset)
            return false;

        if (has_children)
            stack[depth] = this_offset;

        cur_depth = depth;

        return true;
    });

    if (cur_depth == 0) {
        free(stack);
        return 0;
    }

    auto ret = stack[cur_depth - 1];

    free(stack);

    return ret;
}

static bool form_is_string(enum DW_FORM form) {
    switch (form) {
        case DW_FORM::string:
        case DW_FORM::strp:
        case DW_FORM::line_strp:
        case DW_FORM::strp_sup:
        case DW_FORM::strx:
        case DW_FORM::strx1:
        case DW_FORM::strx2:
        case DW_FORM::strx3:
        case DW_FORM::strx4:
            return true;

        default:
            return false;
    }
}

static string_view read_string(span<const uint8_t> data, enum DW_FORM form,
                               const kernel_image& img,
                               optional<uint32_t> str_offsets_base) {
    uint64_t index;

    switch (form) {
        case DW_FORM::string:
            return string_view((char*)data.data(), data.size());

        case DW_FORM::strp:
            return string_view((char*)img.debug_str.data() + *(uint32_t*)data.data());

        case DW_FORM::line_strp:
            print("FIXME - DW_FORM::line_strp\n");
            return string_view();

        case DW_FORM::strp_sup:
            print("FIXME - DW_FORM::strp_sup\n");
            return string_view();

        case DW_FORM::strx:
            index = read_uleb128(data);
            break;

        case DW_FORM::strx1:
            index = data[0];
            break;

        case DW_FORM::strx2:
            index = *(uint16_t*)data.data();
            break;

        case DW_FORM::strx3:
            index = data[0] | (data[1] << 8) | (data[2] << 16);
            break;

        case DW_FORM::strx4:
            index = *(uint32_t*)data.data();
            break;

        default:
            return string_view();
    }

    if (!str_offsets_base.has_value()) {
        print("DW_FORM::strx but no str_offsets_base\n");
        return string_view();
    }

    if (*str_offsets_base + ((index + 1) * sizeof(uint32_t)) > img.debug_str_offsets.size()) {
        print("DW_FORM::strx would overflow .debug_str_offsets section\n");
        return string_view();
    }

    auto list = (uint32_t*)(img.debug_str_offsets.data() + *str_offsets_base);

    return string_view((char*)img.debug_str.data() + list[index]);
}

static void get_func_name(const kernel_image& img, span<const uint8_t> die,
                          span<const uint8_t> att_list,
                          span<const uint8_t*> abbrev_list,
                          span<const uint8_t> cu, span<char>& sp,
                          optional<uint32_t> str_offsets_base) {
    string_view name, linkage_name;
    optional<uint32_t> specification;

    loop_through_atts(die, att_list, [&](enum DW_AT att_num, enum DW_FORM form, span<const uint8_t> data) -> bool {
        if (att_num == DW_AT::name && form_is_string(form))
            name = read_string(data, form, img, str_offsets_base);
        else if (att_num == DW_AT::linkage_name && form_is_string(form))
            linkage_name = read_string(data, form, img, str_offsets_base);
        else if (att_num == DW_AT::specification && form == DW_FORM::ref4)
            specification = *(uint32_t*)data.data();

        return true;
    });

    if (name.empty() && !linkage_name.empty())
        name = linkage_name;

    if (!name.empty()) {
        sp = format(sp, "{}", name);
        return;
    }

    // if class function, we need to follow the specification
    if (!specification.has_value()) {
        sp = format(sp, "???");
        return;
    }

    uint32_t ref = *specification;
    size_t written = 0;

    static const char sep[] = { ':', ':' };

    do {
        name = string_view();

        find_die_by_reference(cu, ref, abbrev_list, [&name, &linkage_name, &img, &str_offsets_base]
                (enum DW_TAG tag, span<const uint8_t> die, span<const uint8_t> att_list) {
            if (tag == DW_TAG::compile_unit)
                return;

            loop_through_atts(die, att_list, [&name, &linkage_name, &img, &str_offsets_base]
                    (enum DW_AT att_num, enum DW_FORM form, span<const uint8_t> data) -> bool {
                if (att_num == DW_AT::name && form_is_string(form))
                    name = read_string(data, form, img, str_offsets_base);
                else if (att_num == DW_AT::linkage_name && form_is_string(form))
                    linkage_name = read_string(data, form, img, str_offsets_base);

                return true;
            });
        });

        if (name.empty())
            break;

        bool write_sep = written != 0;

        // FIXME - only need to move what we've written, not whole buffer
        if (name.size() < sp.size()) {
            size_t len = name.size() + (write_sep ? sizeof(sep) : 0);

            memmove(sp.data() + len, sp.data(), sp.size() - len);
        }

        memcpy(sp.data(), name.data(), min(name.size(), sp.size()));
        written += name.size() + (write_sep ? sizeof(sep) : 0);

        if (write_sep && sp.size() > name.size())
            memcpy(sp.data() + name.size(), sep, min(sizeof(sep), sp.size() - name.size()));

        auto ref2 = find_die_parent(cu, abbrev_list, ref);
        if (ref2 == 0)
            break;

        ref = ref2;
    } while (true);

    if (written == 0) {
        if (name.empty() && !linkage_name.empty())
            name = linkage_name;

        if (name.empty())
            sp = format(sp, "???");

        return;
    }

    sp = span(sp.data() + written, written > sp.size() ? 0 : sp.size() - written);
}

static bool form_is_address(enum DW_FORM form) {
    return form == DW_FORM::addr || form == DW_FORM::addrx ||
           form == DW_FORM::addrx1 || form == DW_FORM::addrx2 ||
           form == DW_FORM::addrx3 || form == DW_FORM::addrx4;
}

static optional<uint64_t> read_address(span<const uint8_t> data, enum DW_FORM form,
                                       span<const uint8_t> debug_addr,
                                       optional<uint32_t> addr_base) {
    uint64_t index;

    switch (form) {
        case DW_FORM::addr:
            return *(uint64_t*)data.data();

        case DW_FORM::addrx:
            index = read_uleb128(data);
            break;

        case DW_FORM::addrx1:
            index = data[0];
            break;

        case DW_FORM::addrx2:
            index = *(uint16_t*)data.data();
            break;

        case DW_FORM::addrx3:
            index = data[0] | (data[1] << 8) | (data[2] << 16);
            break;

        case DW_FORM::addrx4:
            index = *(uint32_t*)data.data();
            break;

        default:
            break;
    }

    if (!addr_base.has_value()) {
        print("DW_FORM::addrx specified but no DW_AT::addr_base given\n");
        return nullopt;
    }

    // FIXME - check .debug_addr header that address_size == 8
    // FIXME - check .debug_addr header than segment_selector_size == 0

    if (*addr_base + ((index + 1) * sizeof(uint64_t)) > debug_addr.size()) {
        print("DW_FORM::addrx would overflow .debug_addr section\n");
        return false;
    }

    auto list = (uint64_t*)(debug_addr.data() + *addr_base);

    if (list[index] == 0)
        return nullopt;

    return list[index];
}

uint64_t dwarf_find_function2(const kernel_image& img, uint64_t addr,
                              span<const uint8_t> cu, span<const uint8_t> abbrev,
                              span<char>& sp, optional<uint32_t> addr_base,
                              optional<uint32_t> str_offsets_base) {
    uint64_t func_start = 0;

    auto abbrev_list = load_abbrev_entries(abbrev);

    if (abbrev_list.empty())
        return 0;

    loop_through_dies(cu, abbrev_list, [&](enum DW_TAG tag, bool, span<const uint8_t> die,
                                           span<const uint8_t> att_list, unsigned int depth,
                                           uint32_t) -> bool {
        optional<uint64_t> low_pc, high_pc, func_length;

        if (tag != DW_TAG::subprogram || depth != 1)
            return true;

        auto orig_die = die;

        loop_through_atts(die, att_list, [&](enum DW_AT att_num, enum DW_FORM form, span<const uint8_t> data) -> bool {
            if (att_num == DW_AT::low_pc && form_is_address(form))
                low_pc = read_address(data, form, img.debug_addr, addr_base);
            else if (att_num == DW_AT::high_pc) {
                if (form_is_address(form))
                    high_pc = read_address(data, form, img.debug_addr, addr_base);
                else if (form == DW_FORM::data8) // FIXME - generic integer-reading code
                    func_length = *(uint64_t*)data.data();
                else if (form == DW_FORM::data4) // FIXME - generic integer-reading code
                    func_length = *(uint32_t*)data.data();
            }

            return true;
        });

        if (!high_pc.has_value() && low_pc.has_value() && func_length.has_value())
            high_pc = *low_pc + *func_length;

        if (!low_pc.has_value() || !high_pc.has_value())
            return true;

        if (addr < *low_pc || addr >= *high_pc)
            return true;

        die = orig_die;

        get_func_name(img, die, att_list, abbrev_list, cu, sp, str_offsets_base);

        func_start = *low_pc;
        return false;
    });

    free(abbrev_list.data());

    return func_start;
}

uint64_t dwarf_find_function(const kernel_image& img, uint64_t addr,
                             span<char>& sp) {
    auto info = img.debug_info;
    if (info.empty())
        return 0;

    auto abbrev = img.debug_abbrev;
    if (abbrev.empty())
        return 0;

    auto rnglists = img.debug_rnglists;

    span<const uint8_t> found_cu;
    uint32_t found_debug_abbrev_object;
    optional<uint32_t> addr_base, str_offsets_base;

    // find CU with addr in DW_AT_ranges
    loop_through_cus(info, [&](span<const uint8_t> cu, uint32_t debug_abbrev_object) -> bool {
        auto current_cu = cu;

        auto abbrev_list = load_abbrev_entries(abbrev.subspan(debug_abbrev_object));

        if (abbrev_list.empty())
            return true;

        optional<uint64_t> low_pc, high_pc, rnglistx;
        optional<uint32_t> ranges_offset, rnglist_base;

        addr_base = nullopt;
        str_offsets_base = nullopt;

        loop_through_dies(cu, abbrev_list, [&](enum DW_TAG tag, bool, span<const uint8_t> die,
                                               span<const uint8_t> att_list, unsigned int,
                                               uint32_t) -> bool {
            if (tag != DW_TAG::compile_unit)
                return false;

            loop_through_atts(die, att_list, [&](enum DW_AT att_num, enum DW_FORM form, span<const uint8_t> data) {
                if (att_num == DW_AT::ranges) {
                    if (form == DW_FORM::sec_offset)
                        ranges_offset = *(uint32_t*)data.data();
                    else if (form == DW_FORM::rnglistx)
                        rnglistx = read_uleb128(data);
                } else if (att_num == DW_AT::low_pc && form == DW_FORM::addr)
                    low_pc = *(uint64_t*)data.data();
                else if (att_num == DW_AT::high_pc && form == DW_FORM::udata)
                    high_pc = read_uleb128(data);
                else if (att_num == DW_AT::rnglists_base && form == DW_FORM::sec_offset)
                    rnglist_base = *(uint32_t*)data.data();
                else if (att_num == DW_AT::addr_base && form == DW_FORM::sec_offset)
                    addr_base = *(uint32_t*)data.data();
                else if (att_num == DW_AT::str_offsets_base && form == DW_FORM::sec_offset)
                    str_offsets_base = *(uint32_t*)data.data();

                return true;
            });

            if (ranges_offset.has_value()) {
                if (addr_in_range(addr, rnglists.subspan(*ranges_offset),
                                  img.debug_addr, addr_base)) {
                    found_cu = current_cu;
                    found_debug_abbrev_object = debug_abbrev_object;
                }
            }

            return false;
        });

        if (found_cu.empty()) {
            if (rnglist_base.has_value() && rnglistx.has_value()) {
                // FIXME - check rnglist has DWARF header
                // FIXME - check *rnglistx < offset_entry_count in header

                auto offset = *(uint32_t*)(rnglists.data() + *rnglist_base + (*rnglistx * sizeof(uint32_t)));

                if (addr_in_range(addr, rnglists.subspan(*rnglist_base + offset),
                                  img.debug_addr, addr_base)) {
                    found_cu = current_cu;
                    found_debug_abbrev_object = debug_abbrev_object;
                }
            } else if (low_pc.has_value() && high_pc.has_value()) {
                if (addr >= *low_pc && addr < *low_pc + *high_pc) {
                    found_cu = current_cu;
                    found_debug_abbrev_object = debug_abbrev_object;
                }
            }
        }

        free(abbrev_list.data());

        if (!found_cu.empty())
            return false;

        return true;
    });

    if (found_cu.empty())
        return 0;

    return dwarf_find_function2(img, addr, found_cu,
                                abbrev.subspan(found_debug_abbrev_object),
                                sp, addr_base, str_offsets_base);
}
