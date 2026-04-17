module;

#include <string_view>
#include <span>
#include <stdint.h>

export module smbios_parser;

import loaderblock;
import print;
import smbios;
import mem;

using namespace std;

static span<const uint8_t> smbios_data;
export string_view smbios_manufacturer, smbios_product_name;

static string_view find_smbios_string(span<const uint8_t> tbl, size_t index) {
    auto& h = *(smbios_header*)tbl.data();
    auto d = span(tbl.data() + h.length, tbl.size() - h.length);

    size_t num = 1;

    while (true) {
        auto sv = string_view((char*)d.data());

        if (num == index)
            return sv;

        d = span(d.data() + sv.size() + 1, d.size() - sv.size() - 1);
        num++;

        if (d.empty())
            break;
    }

    return "";
}

static void parse_smbios_table(span<const uint8_t> tbl) {
    auto& h = *(smbios_header*)tbl.data();

    if (h.type != smbios_type::system_information)
        return;

    auto& sysinfo = *(smbios_system_information*)tbl.data();

    if (h.length < offsetof(smbios_system_information, uuid))
        return;

    if (sysinfo.manufacturer != 0)
        smbios_manufacturer = find_smbios_string(tbl, sysinfo.manufacturer);

    if (sysinfo.product_name != 0)
        smbios_product_name = find_smbios_string(tbl, sysinfo.product_name);

    if (!smbios_manufacturer.empty() && !smbios_product_name.empty())
        print("SMBIOS: {} {}\n", smbios_manufacturer, smbios_product_name);
    else if (smbios_manufacturer.empty())
        print("SMBIOS: {}\n", smbios_product_name);
    else if (smbios_product_name.empty())
        print("SMBIOS: {}\n", smbios_manufacturer);
    else
        print("SMBIOS: no manufacturer or product name found\n");
}

export void init_smbios(const loader_block& lb) {
    if (lb.smbios_address == 0 || lb.smbios_length == 0) {
        print("SMBIOS not found\n");
        return;
    }

    smbios_data = span((uint8_t*)phys_address(lb.smbios_address), lb.smbios_length);

    auto d = smbios_data;

    while (d.size() > sizeof(smbios_header)) {
        auto& h = *(smbios_header*)d.data();

        if (h.type == smbios_type::end)
            break;

        auto* table_start = d.data();

        if (d.size() < h.length)
            break;

        d = span(d.data() + h.length, d.size() - h.length);

        if (d.size() < 2)
            break;

        bool found_end = false;
        for (size_t i = 1; i < d.size(); i++) {
            if (d[i] == 0 && d[i - 1] == 0) {
                d = span(d.data() + i + 1, d.size() - i - 1);
                found_end = true;
                break;
            }
        }

        if (!found_end)
            break;

        parse_smbios_table(span(table_start, (size_t)(d.data() - table_start)));
    }
}
