#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string_view>
#include <map>
#include <span>

import nt;
import print;
import evdev;
import unique_handle;
import moxnox_ioctls;

using namespace std;

static map<pair<uint32_t, evdev_abs_code>, pair<int32_t, int32_t>> abs_ranges;
static uint32_t fb_width, fb_height, fb_pixels_per_scan_line;
static uint32_t mouse_x, mouse_y;
static uint32_t old_mouse_x, old_mouse_y, old_colour;
static span<uint32_t> framebuffer;

static pair<int32_t, int32_t> find_abs_range(HANDLE h, uint32_t id,
                                             evdev_abs_code code) {
    if (auto it = abs_ranges.find(make_pair(id, code)); it != abs_ranges.end())
        return it->second;

    MOXNOX_IOCTL_EVDEV_MULTIPLEXER_GET_ABSINFO input;
    IO_STATUS_BLOCK iosb;
    input_absinfo ai;

    input.id = id;
    input.code = (uint32_t)code;

    auto status = NtDeviceIoControlFile(h, 0, nullptr, nullptr, &iosb,
                                        IOCTL_MOXNOX_EVDEV_GET_ABSINFO,
                                        &input, sizeof(input), &ai, sizeof(ai));
    if (!success(status)) {
        print_stderr("IOCTL_MOXNOX_EVDEV_GET_ABSINFO returned {}\n",
                     status);
        return {0, 0};
    }

    abs_ranges[make_pair(id, code)] = make_pair(ai.minimum, ai.maximum);

    return {ai.minimum, ai.maximum};
}

static void handle_mouse_abs(HANDLE mouse_pipe, const input_event_with_id& ev) {
    auto code = (evdev_abs_code)ev.ev.code;

    if (code != evdev_abs_code::ABS_X && code != evdev_abs_code::ABS_Y)
        return;

    auto [min, max] = find_abs_range(mouse_pipe, ev.id, code);

    if (min == max)
        return;

    if (code == evdev_abs_code::ABS_X)
        mouse_x = ((ev.ev.value - min) * fb_width) / (max - min);
    else if (code == evdev_abs_code::ABS_Y)
        mouse_y = ((ev.ev.value - min) * fb_height) / (max - min);
}

static void handle_syn_report() {
    void* flush_addr = framebuffer.data();
    size_t flush_size = framebuffer.size_bytes();
    IO_STATUS_BLOCK iosb;

    if (mouse_x == old_mouse_x && mouse_y == old_mouse_y)
        return;

    framebuffer[(old_mouse_y * fb_pixels_per_scan_line) + old_mouse_x] = old_colour;

    auto& px = framebuffer[(mouse_y * fb_pixels_per_scan_line) + mouse_x];

    old_colour = px;
    px = 0xff0000;

    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;

    auto status = NtFlushVirtualMemory(NtCurrentProcess(), &flush_addr,
                                       &flush_size, &iosb);
    if (!success(status)) {
        print_stderr("NtFlushVirtualMemory returned {}\n", status);
        return;
    }
}

static void handle_mouse_event(HANDLE mouse_pipe, const input_event_with_id& ev) {
    switch (ev.ev.type) {
        case evdev_type::EV_ABS:
            handle_mouse_abs(mouse_pipe, ev);
            break;

        // FIXME - also handle EV_REL

        case evdev_type::EV_SYN:
            if ((evdev_syn_code)ev.ev.code == evdev_syn_code::SYN_REPORT)
                handle_syn_report();
            break;

        default:
            break;
    }
}

int main() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    HANDLE h;
    MN_SECTION_FRAMEBUFFER_INFORMATION fb_info;
    void* addr;
    size_t size, return_length;

    static const u16string_view fn = u"/dev/mice";
    static const u16string_view fb = u"/dev/framebuffer";

    us.Length = us.MaximumLength = (uint16_t)(fn.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fn.data();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return 1;
    }

    unique_handle mouse_pipe{h};

    us.Length = us.MaximumLength = (uint16_t)(fb.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)fb.data();

    status = NtOpenSection(&h, SECTION_MAP_READ, &oa);
    if (!success(status)) {
        print_stderr("NtOpenSection returned {}\n", status);
        return 1;
    }

    unique_handle sect{h};

    status = NtQuerySection(sect.get(), SECTION_INFORMATION_CLASS::MoxnoxFramebufferInformation,
                            &fb_info, sizeof(fb_info), &return_length);
    if (!success(status)) {
        print_stderr("NtQuerySection returned {}\n", status);
        return 1;
    }

    fb_width = fb_info.width;
    fb_height = fb_info.height;
    fb_pixels_per_scan_line = fb_info.pixels_per_scan_line;

    mouse_x = fb_width / 2;
    mouse_y = fb_height / 2;

    old_mouse_x = mouse_x;
    old_mouse_y = mouse_y;

    addr = nullptr;
    size = 0;

    status = NtMapViewOfSection(sect.get(), NtCurrentProcess(), &addr, 0, 0,
                                nullptr, &size, SECTION_INHERIT::ViewUnmap, 0,
                                PAGE_READWRITE);
    if (!success(status)) {
        print_stderr("NtMapViewOfSection returned {}\n", status);
        return 1;
    }

    framebuffer = span((uint32_t*)addr, size / sizeof(uint32_t));

    old_colour = framebuffer[(mouse_y * fb_pixels_per_scan_line) + mouse_x];

    while (true) {
        input_event_with_id ev;

        status = NtReadFile(mouse_pipe.get(), 0, nullptr, nullptr,
                            &iosb, &ev, sizeof(ev), nullptr, nullptr);

        if (status == nt_status::ALERTED)
            continue;
        else if (!success(status)) {
            print_stderr("NtReadFile returned {}\n", status);
            return 1;
        }

        handle_mouse_event(mouse_pipe.get(), ev);

#if 0
        print("tv_sec = {}, tv_usec = {}, type = {}, code = ",
              +ev.ev.time.tv_sec, +ev.ev.time.tv_usec, (evdev_type)ev.ev.type);

        switch (ev.ev.type) {
            case evdev_type::EV_KEY:
                print("{}", (evdev_key_code)ev.ev.code);
            break;

            case evdev_type::EV_SYN:
                print("{}", (evdev_syn_code)ev.ev.code);
            break;

            case evdev_type::EV_REL:
                print("{}", (evdev_rel_code)ev.ev.code);
            break;

            case evdev_type::EV_ABS:
                print("{}", (evdev_abs_code)ev.ev.code);
            break;

            case evdev_type::EV_LED:
                print("{}", (evdev_led_code)ev.ev.code);
            break;

            default:
                print("{}", (evdev_code)ev.ev.code);
            break;
        }

        print(", value = {}", +ev.ev.value);

        if (ev.ev.type == evdev_type::EV_ABS) {
            auto [min, max] = find_abs_range(mouse_pipe.get(), ev.id, ev.ev.code);

            print(" (min {}, max {})", min, max);
        }

        print("\n");
#endif
    }

    NtUnmapViewOfSection(NtCurrentProcess(), addr);
}
