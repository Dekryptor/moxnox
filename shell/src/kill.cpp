#include <span>
#include <string_view>
#include <string.h>

import nt;
import ntdll;
import print;
import unique_handle;

using namespace std;

int main(int argc, char** argv) {
    if (argc == 1) {
        print_stderr("usage: kill pid\n");
        return 1;
    }

    for (int i = 1; i < argc; i++) {
        auto arg = string_view(argv[i]);
        bool valid = true;
        uintptr_t pid = 0;
        OBJECT_ATTRIBUTES oa;
        HANDLE h;
        CLIENT_ID client_id;
        unique_handle proc;

        if (arg.empty())
            valid = false;
        else {
            while (!arg.empty()) {
                pid *= 10;

                if (arg.front() >= '0' && arg.front() <= '9')
                    pid += arg.front() - '0';
                else {
                    valid = false;
                    break;
                }

                arg = string_view(arg.data() + 1, arg.size() - 1);
            }
        }

        if (!valid) {
            print_stderr("kill: {}: arguments must be process IDs\n", argv[i]);
            continue;
        }

        client_id.UniqueProcess = pid;
        client_id.UniqueThread = 0;

        memset(&oa, 0, sizeof(oa));
        oa.Length = sizeof(OBJECT_ATTRIBUTES);

        auto status = NtOpenProcess(&h, PROCESS_TERMINATE, &oa, &client_id);
        if (status == nt_status::INVALID_CID) {
            print_stderr("kill: ({}) - no such process\n", pid);
            continue;
        } else if (!success(status)) {
            print_stderr("NtOpenProcess returned {}\n", status);
            continue;
        }

        proc.reset(h);

        status = NtTerminateProcess(proc.get(), nt_status::CONTROL_C_EXIT);
        if (!success(status))
            print_stderr("NtTerminateProcess returned {}\n", status);
    }

    // FIXME - different status if we fail?

    return 0;
}
