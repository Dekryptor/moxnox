module;

#include <string_view>
#include <array>
#include <stdint.h>

export module console;

import object_ref;
import loaderblock;

using namespace std;

export array<uint8_t, 128> edid;

export bool have_edid;
export object_ref console_chardev;
export object_ref com1_pipe_handle;
export unsigned int console_cols; // FIXME
export bool userspace_started = false;
export object_ref framebuffer_section;

export enum class console_type {
#ifdef __x86_64__
    text,
#endif
    boot_framebuffer,
    other_framebuffer,
    other
};

export enum console_type console_type;

export void early_print(string_view s);
export void very_early_print(string_view s);
export void safe_print(string_view s);
export bool init_fb_console();
export void copy_edid(loader_block* lb);

#ifdef __x86_64__
export bool init_console(const loader_block_framebuffer& fb,
                         const loader_block_text_console& tc);
#else
export bool init_console(const loader_block_framebuffer& fb);
#endif

