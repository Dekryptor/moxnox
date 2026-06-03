#include <span>
#include <string_view>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <signal.h>

import nt;
import ntdll;
import print;
import unique_handle;
import unicode;

#undef errno
import errno;

using namespace std;

int main(int argc, char** argv) {
    unique_handle proc;
    UNICODE_STRING us;
    uint32_t retlen;
    int ret;

    if (argc <= 1) {
        print_stderr("usage: killall NAME\n");
        return 1;
    }

    auto done = (bool*)malloc(sizeof(bool) * (unsigned int)(argc - 1));
    if (!done) {
        print_stderr("malloc failed\n");
        return 1;
    }

    auto arr = (u16string_view*)malloc((unsigned int)(argc - 1) * sizeof(u16string_view));
    if (!arr) {
        print_stderr("malloc failed\n");
        free(done);
        return 1;
    }

    for (int i = 0; i < argc - 1; i++) {
        auto sv = string_view(argv[i + 1]);

        auto u16len = utf8_to_utf16_len(sv);
        auto u16src = (char16_t*)malloc(u16len * sizeof(char16_t));
        if (!u16src) {
            print_stderr("malloc failed\n");

            for (int j = 0; j < i; j++) {
                free((void*)arr[j].data());
            }

            free(arr);
            free(done);

            return 1;
        }

        utf8_to_utf16(span(u16src, u16len), sv);

        arr[i] = u16string_view(u16src, u16len);
    }

    memset(done, 0, sizeof(bool) * (unsigned int)(argc - 1));

    while (true) {
        HANDLE h;

        auto status = NtGetNextProcess(proc.get(), PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_TERMINATE,
                                       0, 0, &h);
        if (status == nt_status::NO_MORE_ENTRIES)
            break;
        else if (!success(status)) {
            print_stderr("NtGetNextProcess returned {}\n", status);
            ret = 1;
            goto end;
        }

        proc.reset(h);

        status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::ImageFileName,
                                           &us, sizeof(us), &retlen);
        if (!success(status) && status != nt_status::INFO_LENGTH_MISMATCH) {
            print_stderr("NtQueryInformationProcess returned {}\n", status);
            ret = 1;
            goto end;
        }

        auto buffer = malloc(retlen);
        if (!buffer) {
            print_stderr("malloc failed\n");
            ret = 1;
            goto end;
        }

        status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::ImageFileName,
                                           buffer, retlen, &retlen);
        if (!success(status)) {
            print_stderr("NtQueryInformationProcess returned {}\n", status);
            free(buffer);
            ret = 1;
            goto end;
        }

        auto& us = *(UNICODE_STRING*)buffer;

        u16string_view name(us.Buffer, us.Length / sizeof(char16_t));

        if (auto bs = name.find_last_of(u'\\'); bs != u16string_view::npos)
            name = u16string_view(name.data() + bs + 1, name.size() - bs - 1);

        if (auto dot = name.find_last_of(u'.'); dot != u16string_view::npos)
            name = u16string_view(name.data(), dot);

        for (int i = 0; i < argc - 1; i++) {
            if (arr[i] == name) {
                PROCESS_BASIC_INFORMATION pbi;

                done[i] = true;

                auto status = NtQueryInformationProcess(proc.get(), PROCESSINFOCLASS::BasicInformation,
                                                        &pbi, sizeof(pbi), &retlen);
                if (!success(status)) {
                    print_stderr("{}: NtQueryInformationProcess returned {}\n", name, status);
                    break;
                }

                // FIXME - allow other signals

                if (kill((pid_t)pbi.UniqueProcessId, SIGINT) == -1)
                    print_stderr("kill failed (errno {})\n", errno_to_string_view(*_errno()));

                break;
            }
        }

        free(buffer);
    }

    for (int i = 0; i < argc - 1; i++) {
        if (!done[i])
            print_stderr("{}: no process found\n", argv[i + 1]);
    }

    ret = 0;

end:
    for (int i = 0; i < argc - 1; i++) {
        free((void*)arr[i].data());
    }

    free(arr);
    free(done);

    return ret;
}
