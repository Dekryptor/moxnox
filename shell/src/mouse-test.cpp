#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string_view>
#include <map>

import nt;
import print;
import evdev;
import unique_handle;
import moxnox_ioctls;

using namespace std;

static map<pair<uint32_t, uint32_t>, pair<int32_t, int32_t>> abs_ranges;

static pair<int32_t, int32_t> find_abs_range(HANDLE h, uint32_t id,
                                             uint32_t code) {
    if (auto it = abs_ranges.find(make_pair(id, code)); it != abs_ranges.end())
        return it->second;

    MOXNOX_IOCTL_EVDEV_MULTIPLEXER_GET_ABSINFO input;
    IO_STATUS_BLOCK iosb;
    input_absinfo ai;

    input.id = id;
    input.code = code;

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

int main() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    NTSTATUS status;
    HANDLE h;

    static const u16string_view fn = u"/dev/mice";

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
    }
}
