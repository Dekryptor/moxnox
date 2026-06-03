#include <span>
#include <string_view>
#include <string.h>
#include <signal.h>

import print;
import ucrtbase;

#undef errno
import errno;

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

        if (arg.empty())
            valid = false;
        else {
            while (!arg.empty()) {
                pid *= 10;

                if (arg.front() >= '0' && arg.front() <= '9')
                    pid += (unsigned int)(arg.front() - '0');
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

        // FIXME - allow other signals

        if (kill((pid_t)pid, SIGINT) == -1) {
            switch (*_errno()) {
                case ENOENT:
                    print_stderr("kill: ({}) - no such process\n", pid);
                    break;

                default:
                    print_stderr("kill returned {}\n", errno_to_string_view(*_errno()));
                    continue;
            }
        }
    }

    // FIXME - different status if we fail?

    return 0;
}
