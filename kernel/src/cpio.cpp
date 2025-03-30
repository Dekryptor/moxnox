module;

#include <span>
#include <string_view>
#include <string.h>
#include <stdint.h>

export module cpio;

import print;

using namespace std;

struct cpio_entry {
    char signature[6];
    char ino[8];
    char mode[8];
    char uid[8];
    char gid[8];
    char num_links[8];
    char mtime[8];
    char file_size[8];
    char device_major[8];
    char device_minor[8];
    char block_major[8];
    char block_minor[8];
    char path_size[8];
    char checksum[8];
} __attribute((packed));

const char signature[] = { '0', '7', '0', '7', '0', '1' };

uint64_t parse_int(span<char, 8> sp) {
    uint64_t ret = 0;

    for (auto c : sp) {
        ret <<= 4;

        if (c >= '0' && c <= '9')
            ret |= (uint64_t)(c - '0');
        else if (c >= 'a' && c <= 'f')
            ret |= (uint64_t)(c - 'a' + 0xa);
        else if (c >= 'A' && c <= 'F')
            ret |= (uint64_t)(c - 'A' + 0xa);
        else
            return 0;
    }

    return ret;
}

export void process_cpio(span<const uint8_t> data, invocable<string_view, uint64_t, span<const uint8_t>, uint64_t> auto func) {
    do {
        if (data.size() < sizeof(cpio_entry))
            return;

        auto& ent = *(cpio_entry*)data.data();

        // FIXME - support 070702 (also need to check checksum)

        if (memcmp(ent.signature, signature, sizeof(signature))) {
            print("unrecognized cpio signature\n");
            return;
        }

        auto mode = parse_int(ent.mode);
        auto mtime = parse_int(ent.mtime);
        auto file_size = parse_int(ent.file_size);
        auto path_size = parse_int(ent.path_size);

        data = span(data.data() + sizeof(cpio_entry), data.size() - sizeof(cpio_entry));

        if (data.size() < path_size)
            return;

        string_view path((char*)data.data(), path_size);

        if (!path.empty() && path.back() == 0)
            path = string_view(path.data(), path.size() - 1);

        if (path == "TRAILER!!!")
            return;

        unsigned int padding = 4 - ((path_size + 2) % 4);

        if (padding == 4)
            padding = 0;

        if (data.size() < path_size + padding)
            return;

        data = span(data.data() + path_size + padding, data.size() - path_size - padding);

        if (data.size() < file_size)
            return;

        auto contents = span(data.data(), file_size);

        func(path, mode, contents, mtime);

        padding = (file_size % 4) ? (4 - (file_size % 4)) : 0;

        if (data.size() < padding)
            return;

        data = span(data.data() + file_size + padding, data.size() - file_size - padding);
    } while (true);
}
