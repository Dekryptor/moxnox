#include <cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include <string_view>
#include <span>

using namespace std;

import nt;
import unique_handle;
import print;
import evdev;

static span<uint32_t> framebuffer;
static unsigned int fb_width, fb_height, fb_pixels_per_scan_line;

static void flush_framebuffer(cairo_surface_t* surface) {
    void* flush_addr = framebuffer.data();
    size_t flush_size = framebuffer.size_bytes();
    IO_STATUS_BLOCK iosb;

    cairo_surface_flush(surface);

    auto status = NtFlushVirtualMemory(NtCurrentProcess(), &flush_addr,
                                       &flush_size, &iosb);
    if (!success(status)) {
        print_stderr("NtFlushVirtualMemory returned {}\n", status);
        return;
    }
}

static void wait_for_esc() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    HANDLE h;

    static const u16string_view fn = u"/dev/keyboard";

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return;
    }

    unique_handle keyboard_pipe{h};

    while (true) {
        input_event_with_id ev;

        status = NtReadFile(keyboard_pipe.get(), 0, nullptr, nullptr,
                            &iosb, &ev, sizeof(ev), nullptr, nullptr);

        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status)) {
            print_stderr("NtReadFile returned {}\n", status);
            return;
        }

        if (ev.ev.type != evdev_type::EV_KEY)
            continue;

        if ((evdev_key_code)ev.ev.code != evdev_key_code::KEY_ESC)
            continue;

        break;
    }
}

int main() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    unique_handle sect;
    HANDLE h;
    nt_status status;
    MN_SECTION_FRAMEBUFFER_INFORMATION fb_info;
    void* addr = nullptr;
    size_t size, return_length;

    static const u16string_view fb = u"/dev/framebuffer";

    us.Length = us.MaximumLength = (uint16_t)(fb.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fb.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(fb.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fb.data();

    status = NtOpenSection(&h, SECTION_MAP_READ, &oa);
    if (!success(status)) {
        print_stderr("NtOpenSection returned {}\n", status);
        return 1;
    }

    sect.reset(h);

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::MoxnoxFramebufferInformation,
                            &fb_info, sizeof(fb_info), &return_length);
    if (!success(status)) {
        print_stderr("NtQuerySection returned {}\n", status);
        return 1;
    }

    status = NtMapViewOfSection(sect.get(), NtCurrentProcess(), &addr, 0, 0,
                                nullptr, &size, SECTION_INHERIT::ViewUnmap, 0,
                                PAGE_READWRITE);
    if (!success(status)) {
        print_stderr("NtMapViewOfSection returned {}\n", status);
        return 1;
    }

    framebuffer = span((uint32_t*)addr, size / sizeof(uint32_t));

    fb_width = fb_info.width;
    fb_height = fb_info.height;
    fb_pixels_per_scan_line = fb_info.pixels_per_scan_line;

    cairo_surface_t* surface = cairo_image_surface_create_for_data(
        (unsigned char*)framebuffer.data(),
        CAIRO_FORMAT_RGB24,
        fb_width,
        fb_height,
        (int)(fb_pixels_per_scan_line * 4)
    );

    if (cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Failed to create Cairo surface\n");
        return 1;
    }

    cairo_t* cr = cairo_create(surface);

    // clear to white
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
    cairo_paint(cr);

    // draw red rectangle
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0);
    cairo_rectangle(cr, 50, 50, 200, 100);
    cairo_fill(cr);

    // draw green circle
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0);
    cairo_arc(cr, 400, 200, 80, 0, 2 * M_PI);
    cairo_fill(cr);

    // draw blue line
    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
    cairo_set_line_width(cr, 10);
    cairo_move_to(cr, 100, 300);
    cairo_line_to(cr, 500, 400);
    cairo_stroke(cr);

    // draw text
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL,
                           CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, 40);
    cairo_move_to(cr, 100, 500);
    cairo_show_text(cr, "Hello Cairo!");

    flush_framebuffer(surface);

    wait_for_esc();

    // cleanup
    cairo_destroy(cr);
    cairo_surface_destroy(surface);

    NtUnmapViewOfSection(NtCurrentProcess(), addr);

    printf("Cairo test complete!\n");
    return 0;
}
