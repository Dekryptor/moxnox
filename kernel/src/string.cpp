#include "kernel.h"

using namespace std;

span<char> sp_strcpy(span<char> dest, string_view src) {
    auto to_copy = min(src.size(), dest.size());

    memcpy(dest.data(), src.data(), to_copy);

    return dest.subspan(to_copy);
}

span<char> sp_itoa_hex(span<char> dest, uint64_t num, unsigned int padding) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0 && padding == 0) {
        dest[0] = '0';
        return dest.subspan(1);
    }

    for (unsigned int i = 0; i < 16; i++) {
        if (!first || padding == 16 - i || num & 0xf000000000000000) {
            uint64_t n = num >> 60;

            if (n <= 9)
                dest[0] = (char)(n + '0');
            else
                dest[0] = (char)(n - 0xa + 'a');

            dest = dest.subspan(1);
            first = false;

            if (dest.empty())
                return dest;
        }

        num <<= 4;
    }

    return dest;
}

size_t utf16_to_utf8_len(u16string_view u16sv) {
    size_t len = 0;

    for (unsigned int i = 0; i < u16sv.size(); i++) {
        uint32_t cp;

        if (u16sv[i] < 0xd800 || u16sv[i] > 0xdfff)
            cp = u16sv[i];
        else if (u16sv[i] >= 0xd800 && u16sv[i] < 0xdc00) {
            if (i == u16sv.size() - 1)
                return 0;

            auto next = u16sv[i+1];

            if (next < 0xdc00 || next > 0xdfff)
                return 0;

            cp = (((uint32_t)u16sv[i] & 0x3ff) << 10) | (next & 0x3ff);

            i++;
        } else // invalid
            return 0;

        if (cp < 0x80)
            len++;
        else if (cp < 0x800)
            len += 2;
        else if (cp < 0x10000)
            len += 3;
        else
            len += 4;
    }

    return len;
}

void utf16_to_utf8(span<char> buf, u16string_view u16sv) {
    if (buf.empty())
        return;

    for (unsigned int i = 0; i < u16sv.size(); i++) {
        uint32_t cp;

        if (u16sv[i] < 0xd800 || u16sv[i] > 0xdfff)
            cp = u16sv[i];
        else if (u16sv[i] >= 0xd800 && u16sv[i] < 0xdc00) {
            if (i == u16sv.size() - 1)
                return;

            auto next = u16sv[i+1];

            if (next < 0xdc00 || next > 0xdfff)
                return;

            cp = (((uint32_t)u16sv[i] & 0x3ff) << 10) | (next & 0x3ff);

            i++;
        } else // invalid
            return;

        if (cp < 0x80) {
            buf[0] = (char)cp;
            buf = buf.subspan(1);
        } else if (cp < 0x800) {
            if (buf.size() < 2)
                return;

            buf[0] = (char)(0xc0 | ((cp & 0x7c0) >> 6));
            buf[1] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(2);
        } else if (cp < 0x10000) {
            if (buf.size() < 3)
                return;

            buf[0] = (char)(0xe0 | ((cp & 0xf000) >> 12));
            buf[1] = (char)(0x80 | ((cp & 0xfc0) >> 6));
            buf[2] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(3);
        } else {
            if (buf.size() < 4)
                return;

            buf[0] = (char)(0xf0 | ((cp & 0xe0000) >> 18));
            buf[1] = (char)(0x80 | ((cp & 0x3f000) >> 12));
            buf[2] = (char)(0x80 | ((cp & 0xfc0) >> 6));
            buf[3] = (char)(0x80 | (cp & 0x3f));

            buf = buf.subspan(4);
        }
    }
}
