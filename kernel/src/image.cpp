module;

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <span>

export module image;

import list;
import bsod;
import section;

using namespace std;

extern "C" uint8_t __image_base__;

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
    char name[0];
};

export list_entry kernel_images = { &kernel_images, &kernel_images };

export void init_images_list(span<const uint8_t> debug_info,
                             span<const uint8_t> debug_abbrev,
                             span<const uint8_t> debug_rnglists,
                             span<const uint8_t> debug_str,
                             span<const uint8_t> debug_addr,
                             span<const uint8_t> debug_str_offsets) {
    const char kernel_name[] = "kernel.exe";

    uint64_t kernel_size = get_image_size(&__image_base__);
    if (kernel_size == 0)
        bsod("get_image_size failed for kernel");

    auto img = (struct kernel_image*)malloc(offsetof(struct kernel_image, name) + sizeof(kernel_name));
    if (!img)
        bsod("malloc failed");

    img->base_address = &__image_base__;
    img->size = kernel_size;
    img->debug_info = debug_info;
    img->debug_abbrev = debug_abbrev;
    img->debug_rnglists = debug_rnglists;
    img->debug_str = debug_str;
    img->debug_addr = debug_addr;
    img->debug_str_offsets = debug_str_offsets;
    memcpy(img->name, kernel_name, sizeof(kernel_name));

    insert_tail_list(kernel_images, img->le);
}

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
