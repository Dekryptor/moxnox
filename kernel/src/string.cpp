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

size_t utf8_to_utf16_len(string_view sv) {
    size_t ret = 0;

    if (sv.empty())
        return 0;

    for (unsigned int i = 0; i < sv.size(); i++) {
        uint32_t cp;

        if (!(sv[i] & 0x80))
            cp = sv[i];
        else if ((sv[i] & 0xe0) == 0xc0) {
            if (i == sv.size() - 1 || (sv[i+1] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = ((sv[i] & 0x1f) << 6) | (sv[i+1] & 0x3f);
                i++;
            }
        } else if ((sv[i] & 0xf0) == 0xe0) {
            if (i >= sv.size() - 2 || (sv[i+1] & 0xc0) != 0x80 || (sv[i+2] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = ((sv[i] & 0xf) << 12) | ((sv[i+1] & 0x3f) << 6) | (sv[i+2] & 0x3f);
                i += 2;
            }
        } else if ((sv[i] & 0xf8) == 0xf0) {
            if (i >= sv.size() - 3 || (sv[i+1] & 0xc0) != 0x80 || (sv[i+2] & 0xc0) != 0x80 || (sv[i+3] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = ((sv[i] & 0x7) << 18) | ((sv[i+1] & 0x3f) << 12) | ((sv[i+2] & 0x3f) << 6) | (sv[i+3] & 0x3f);
                i += 3;
            }
        } else
            cp = 0xfffd;

        if (cp > 0x10ffff)
            cp = 0xfffd;

        if (cp <= 0xffff)
            ret++;
        else
            ret += 2;
    }

    return ret;
}

void utf8_to_utf16(span<char16_t> buf, string_view sv) {
    if (buf.empty())
        return;

    for (unsigned int i = 0; i < sv.size(); i++) {
        uint32_t cp;

        if (!(sv[i] & 0x80))
            cp = sv[i];
        else if ((sv[i] & 0xe0) == 0xc0) {
            if (i == sv.size() - 1 || (sv[i+1] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = ((sv[i] & 0x1f) << 6) | (sv[i+1] & 0x3f);
                i++;
            }
        } else if ((sv[i] & 0xf0) == 0xe0) {
            if (i >= sv.size() - 2 || (sv[i+1] & 0xc0) != 0x80 || (sv[i+2] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = ((sv[i] & 0xf) << 12) | ((sv[i+1] & 0x3f) << 6) | (sv[i+2] & 0x3f);
                i += 2;
            }
        } else if ((sv[i] & 0xf8) == 0xf0) {
            if (i >= sv.size() - 3 || (sv[i+1] & 0xc0) != 0x80 || (sv[i+2] & 0xc0) != 0x80 || (sv[i+3] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = ((sv[i] & 0x7) << 18) | ((sv[i+1] & 0x3f) << 12) | ((sv[i+2] & 0x3f) << 6) | (sv[i+3] & 0x3f);
                i += 3;
            }
        } else
            cp = 0xfffd;

        if (cp > 0x10ffff)
            cp = 0xfffd;

        if (cp <= 0xffff) {
            buf[0] = (uint16_t)cp;
            buf = buf.subspan(1);
        } else {
            if (buf.size() < 2)
                return;

            cp -= 0x10000;

            buf[0] = (char16_t)(0xd800 | ((cp & 0xffc00) >> 10));
            buf[1] = (char16_t)(0xdc00 | (cp & 0x3ff));

            buf = buf.subspan(2);
        }

        if (buf.empty())
            return;
    }
}
