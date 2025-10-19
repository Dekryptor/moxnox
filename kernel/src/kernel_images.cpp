module;

#include <stdint.h>
#include <span>

export module kernel_images;

import list;

using namespace std;

export struct kernel_image {
    list_entry le;
    void* base_address;
    uint64_t size;
    span<const uint8_t> debug_info;
    span<const uint8_t> debug_abbrev;
    span<const uint8_t> debug_rnglists;
    span<const uint8_t> debug_str;
    span<const uint8_t> debug_addr;
    span<const uint8_t> debug_str_offsets;
    span<const uint8_t> debug_line;
    span<const uint8_t> debug_line_str;
    char name[0];
};

export list_entry kernel_images = { &kernel_images, &kernel_images };

export kernel_image* get_kernel_image_from_address(uint64_t addr) {
    auto le = kernel_images.next;

    while (le != &kernel_images) {
        auto& ent = containing_record<kernel_image>(le);

        if (addr >= (uintptr_t)ent.base_address && addr < (uintptr_t)ent.base_address + ent.size)
            return &ent;

        le = le->next;
    }

    return nullptr;
}

