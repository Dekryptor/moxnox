#include "shell.h"

using namespace std;

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
            if (buf.empty())
                return;

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

    while (!sv.empty()) {
        if ((uint8_t)sv[0] < 0x80) {
            ret++;
            sv = string_view(sv.data() + 1, sv.size() - 1);
        } else if (((uint8_t)sv[0] & 0xe0) == 0xc0 && (uint8_t)sv.length() >= 2 && ((uint8_t)sv[1] & 0xc0) == 0x80) {
            ret++;
            sv = string_view(sv.data() + 2, sv.size() - 2);
        } else if (((uint8_t)sv[0] & 0xf0) == 0xe0 && (uint8_t)sv.length() >= 3 && ((uint8_t)sv[1] & 0xc0) == 0x80 && ((uint8_t)sv[2] & 0xc0) == 0x80) {
            ret++;
            sv = string_view(sv.data() + 3, sv.size() - 3);
        } else if (((uint8_t)sv[0] & 0xf8) == 0xf0 && (uint8_t)sv.length() >= 4 && ((uint8_t)sv[1] & 0xc0) == 0x80 && ((uint8_t)sv[2] & 0xc0) == 0x80 && ((uint8_t)sv[3] & 0xc0) == 0x80) {
            char32_t cp = (char32_t)(((uint8_t)sv[0] & 0x7) << 18) | (char32_t)(((uint8_t)sv[1] & 0x3f) << 12) | (char32_t)(((uint8_t)sv[2] & 0x3f) << 6) | (char32_t)((uint8_t)sv[3] & 0x3f);

            if (cp > 0x10ffff) {
                ret++;
                sv = string_view(sv.data() + 4, sv.size() - 4);
                continue;
            }

            ret += 2;
            sv = string_view(sv.data() + 4, sv.size() - 4);
        } else {
            ret++;
            sv = string_view(sv.data() + 1, sv.size() - 1);
        }
    }

    return ret;
}

void utf8_to_utf16(span<char16_t> buf, string_view sv) {
    if (buf.empty())
        return;

    while (!sv.empty() && !buf.empty()) {
        if ((uint8_t)sv[0] < 0x80) {
            buf[0] = (uint8_t)sv[0];
            buf = buf.subspan(1);

            sv = string_view(sv.data() + 1, sv.size() - 1);
        } else if (((uint8_t)sv[0] & 0xe0) == 0xc0 && (uint8_t)sv.length() >= 2 && ((uint8_t)sv[1] & 0xc0) == 0x80) {
            char16_t cp = (char16_t)(((uint8_t)sv[0] & 0x1f) << 6) | (char16_t)((uint8_t)sv[1] & 0x3f);

            buf[0] = cp;
            buf = buf.subspan(1);

            sv = string_view(sv.data() + 2, sv.size() - 2);
        } else if (((uint8_t)sv[0] & 0xf0) == 0xe0 && (uint8_t)sv.length() >= 3 && ((uint8_t)sv[1] & 0xc0) == 0x80 && ((uint8_t)sv[2] & 0xc0) == 0x80) {
            char16_t cp = (char16_t)(((uint8_t)sv[0] & 0xf) << 12) | (char16_t)(((uint8_t)sv[1] & 0x3f) << 6) | (char16_t)((uint8_t)sv[2] & 0x3f);

            if (cp >= 0xd800 && cp <= 0xdfff) {
                buf[0] = 0xfffd;
                buf = buf.subspan(1);

                sv = string_view(sv.data() + 3, sv.size() - 3);
                continue;
            }

            buf[0] = cp;
            buf = buf.subspan(1);

            sv = string_view(sv.data() + 3, sv.size() - 3);
        } else if (((uint8_t)sv[0] & 0xf8) == 0xf0 && (uint8_t)sv.length() >= 4 && ((uint8_t)sv[1] & 0xc0) == 0x80 && ((uint8_t)sv[2] & 0xc0) == 0x80 && ((uint8_t)sv[3] & 0xc0) == 0x80) {
            char32_t cp = (char32_t)(((uint8_t)sv[0] & 0x7) << 18) | (char32_t)(((uint8_t)sv[1] & 0x3f) << 12) | (char32_t)(((uint8_t)sv[2] & 0x3f) << 6) | (char32_t)((uint8_t)sv[3] & 0x3f);

            if (cp > 0x10ffff) {
                buf[0] = 0xfffd;
                buf = buf.subspan(1);

                sv = string_view(sv.data() + 4, sv.size() - 4);
                continue;
            }

            cp -= 0x10000;

            buf[0] = (char16_t)(0xd800 | (cp >> 10));
            buf = buf.subspan(1);

            if (buf.empty())
                return;

            buf[0] = (char16_t)(0xdc00 | (cp & 0x3ff));
            buf = buf.subspan(1);

            sv = string_view(sv.data() + 4, sv.size() - 4);
        } else {
            buf[0] = 0xfffd;
            buf = buf.subspan(1);

            sv = string_view(sv.data() + 1, sv.size() - 1);
        }
    }
}
