#include "kernel.h"

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
        char32_t cp;

        if (!(sv[i] & 0x80))
            cp = (char32_t)sv[i];
        else if ((sv[i] & 0xe0) == 0xc0) {
            if (i == sv.size() - 1 || (sv[i+1] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = (char32_t)(((sv[i] & 0x1f) << 6) | (sv[i+1] & 0x3f));
                i++;
            }
        } else if ((sv[i] & 0xf0) == 0xe0) {
            if (i >= sv.size() - 2 || (sv[i+1] & 0xc0) != 0x80 || (sv[i+2] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = (char32_t)(((sv[i] & 0xf) << 12) | ((sv[i+1] & 0x3f) << 6) | (sv[i+2] & 0x3f));
                i += 2;
            }
        } else if ((sv[i] & 0xf8) == 0xf0) {
            if (i >= sv.size() - 3 || (sv[i+1] & 0xc0) != 0x80 || (sv[i+2] & 0xc0) != 0x80 || (sv[i+3] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = (char32_t)(((sv[i] & 0x7) << 18) | ((sv[i+1] & 0x3f) << 12) | ((sv[i+2] & 0x3f) << 6) | (sv[i+3] & 0x3f));
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
        char32_t cp;

        if (!(sv[i] & 0x80))
            cp = (char32_t)sv[i];
        else if ((sv[i] & 0xe0) == 0xc0) {
            if (i == sv.size() - 1 || (sv[i+1] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = (char32_t)(((sv[i] & 0x1f) << 6) | (sv[i+1] & 0x3f));
                i++;
            }
        } else if ((sv[i] & 0xf0) == 0xe0) {
            if (i >= sv.size() - 2 || (sv[i+1] & 0xc0) != 0x80 || (sv[i+2] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = (char32_t)(((sv[i] & 0xf) << 12) | ((sv[i+1] & 0x3f) << 6) | (sv[i+2] & 0x3f));
                i += 2;
            }
        } else if ((sv[i] & 0xf8) == 0xf0) {
            if (i >= sv.size() - 3 || (sv[i+1] & 0xc0) != 0x80 || (sv[i+2] & 0xc0) != 0x80 || (sv[i+3] & 0xc0) != 0x80)
                cp = 0xfffd;
            else {
                cp = (char32_t)(((sv[i] & 0x7) << 18) | ((sv[i+1] & 0x3f) << 12) | ((sv[i+2] & 0x3f) << 6) | (sv[i+3] & 0x3f));
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

extern "C"
int toupper(int c) {
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 'A';
    else
        return c;
}

extern "C"
int tolower(int c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A' + 'a';
    else
        return c;
}

extern "C"
int isprint(int ch) {
    return ch >= ' ' && ch <= '~' ? 1 : 0;
}

extern "C"
int isxdigit(int ch) {
    if (ch >= '0' && ch <= '9')
        return 1;
    else if (ch >= 'a' && ch <= 'f')
        return 1;
    else if (ch >= 'A' && ch <= 'F')
        return 1;
    else
        return 0;
}

extern "C"
int isdigit(int ch) {
    return ch >= '0' && ch <= '9' ? 1 : 0;
}

extern "C"
int isspace(int ch) {
    switch (ch) {
        case ' ':
        case '\f':
        case '\n':
        case '\r':
        case '\t':
        case '\v':
            return 1;

        default:
            return 0;
    }
}

extern "C"
char* strcat(char* dest, const char* src) {
    auto orig_dest = dest;

    while (dest[0] != 0) {
        dest++;
    }

    memcpy(dest, src, strlen(src) + 1);

    return orig_dest;
}

extern "C"
char* strncpy(char* dst, const char* src, size_t dsize) {
    char* orig_dst = dst;

    while (dsize > 0) {
        if (*src == 0)
            break;

        *dst = *src;
        dst++;
        src++;
        dsize--;
    }

    while (dsize > 0) {
        *dst = 0;
        dst++;
        dsize--;
    }

    return orig_dst;
}
