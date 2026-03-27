#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string_view>

import nt;
import print;
import evdev;
import unique_handle;

using namespace std;

int main() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
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
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    status = NtCreateFile(&h, FILE_READ_DATA, &oa, &iosb, nullptr,
                          FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,
                          0, nullptr, 0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {}\n", status);
        return 1;
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
            return 1;
        }

        print("tv_sec = {}, tv_usec = {}, type = {}, code = {}, value = {}, id = {}\n",
              +ev.ev.time.tv_sec, +ev.ev.time.tv_usec, (evdev_type)ev.ev.type,
              (evdev_code)ev.ev.code, +ev.ev.value, +ev.id);
    }
}
