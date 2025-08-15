#include <ntdll.h>
#include <string_view>

import shelllib;
import print;

using namespace std;

extern "C"
__attribute__((force_align_arg_pointer)) void startup() {
    // FIXME

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
