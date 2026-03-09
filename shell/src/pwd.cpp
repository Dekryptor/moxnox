#include <string_view>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>

using namespace std;

import nt;
import print;

int main() {
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
        return 1;
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        print_stderr("malloc failed\n");
        return 1;
    }

    status = NtQueryInformationFile(params.CurrentDirectory.Handle, &iosb,
                                    fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        print_stderr("pwd: NtQueryInformationFile returned {}\n", status);
        free(fni2);
        return 1;
    }

    auto sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

    print("{}\n", sv);

    free(fni2);

    return 0;
}
