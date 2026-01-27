#include <string_view>
#include <span>
#include <utility>
#include <stdlib.h>
#include <stdint.h>

using namespace std;

import nt;
import ntdll;
import print;
import unicode;

#ifdef __x86_64__
#define FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define FORCE_ALIGN_ARG_POINTER
#endif

extern "C" FORCE_ALIGN_ARG_POINTER void startup() {
    const auto& teb = *NtCurrentTeb();
    const auto& peb = *teb.ProcessEnvironmentBlock;
    const auto& params = *peb.ProcessParameters;

    nt_status status;
    IO_STATUS_BLOCK iosb;
    FILE_NAME_INFORMATION fni;

    status = NtQueryInformationFile(params.CurrentDirectory.Handle, &iosb,
                                    &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        print_stderr("pwd: NtQueryInformationFile returned {}\n", status);
        NtTerminateThread(NtCurrentThread(), status);
        unreachable();
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        print_stderr("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    status = NtQueryInformationFile(params.CurrentDirectory.Handle, &iosb,
                                    fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        print_stderr("pwd: NtQueryInformationFile returned {}\n", status);
        free(fni2);
        NtTerminateThread(NtCurrentThread(), status);
        unreachable();
    }

    auto sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

    print("{}\n", sv);

    free(fni2);

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
