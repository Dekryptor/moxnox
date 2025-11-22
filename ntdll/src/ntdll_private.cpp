module;

#include <stdint.h>
#include <string_view>
#include <span>

export module ntdll_private;

import ntdll;
import crt;

export {

const size_t PAGE_SIZE = 4096;
const uint64_t PAGE_SHIFT = 12;

constexpr uint32_t bytes_to_pages(size_t bytes) {
    return (uint32_t)((bytes + PAGE_SIZE - 1) / PAGE_SIZE);
}

struct ci_char_traits : public std::char_traits<char16_t> {
    static bool eq(char16_t c1, char16_t c2) {
        return toupper(c1) == toupper(c2);
    }

    static bool ne(char16_t c1, char16_t c2) {
        return !eq(c1, c2);
    }

    static bool lt(char16_t c1, char16_t c2) {
        return toupper(c1) < toupper(c2);
    }

    static int compare(const char16_t* s1, const char16_t* s2, size_t n) {
        while (n > 0) {
            auto c1 = toupper(*s1);
            auto c2 = toupper(*s2);

            if (c1 < c2)
                return -1;
            else if (c2 < c1)
                return 1;

            n--;
            s1++;
            s2++;
        }

        return 0;
    }

    static const char16_t* find(const char16_t* s, int n, char16_t a) {
        while (n > 0 && toupper(*s) != toupper(a)) {
            s++;
            n--;
        }

        return s;
    }
};

using u16string_view_ci = std::basic_string_view<char16_t, ci_char_traits>;

struct ldr_entry {
    LDR_DATA_TABLE_ENTRY ldr;
    uint64_t offset_debug_info;
    uint64_t size_debug_info;
    uint64_t offset_debug_abbrev;
    uint64_t size_debug_abbrev;
    uint64_t offset_debug_rnglists;
    uint64_t size_debug_rnglists;
    uint64_t offset_debug_str;
    uint64_t size_debug_str;
    uint64_t offset_debug_addr;
    uint64_t size_debug_addr;
    uint64_t offset_debug_str_offsets;
    uint64_t size_debug_str_offsets;
    uint64_t offset_debug_line;
    uint64_t size_debug_line;
    uint64_t offset_debug_line_str;
    uint64_t size_debug_line_str;
};

}
