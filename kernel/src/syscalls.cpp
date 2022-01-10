#include "kernel.h"
#include "nt.h"
#include "objects.h"

using namespace std;

static NTSTATUS NtDisplayString(const UNICODE_STRING* string) {
    auto u16sv = u16string_view(string->Buffer, string->Length / sizeof(char16_t));
    auto buflen = utf16_to_utf8_len(u16sv);

    if (buflen == 0)
        return nt_status::SUCCESS;

    auto buf = (char*)malloc(buflen);
    if (!buf)
        return nt_status::INSUFFICIENT_RESOURCES;

    utf16_to_utf8(span(buf, buflen), u16sv);

    print(string_view(buf, buflen));

    free(buf);

    return nt_status::SUCCESS;
}

static bool copy_from_process(const void* us_addr, void* ks_addr, size_t size) {
    auto t = get_current_thread();

    return t->proc->copy_from(us_addr, ks_addr, size);
}

static bool copy_unicode_string_from_process(const UNICODE_STRING* us_us, UNICODE_STRING& ks_us) {
    if (!copy_from_process(us_us, &ks_us, sizeof(UNICODE_STRING)))
        return false;

    if (ks_us.Buffer) {
        auto buf = (char16_t*)malloc(ks_us.Length);
        if (!buf)
            return false;

        if (!copy_from_process(ks_us.Buffer, buf, ks_us.Length)) {
            free(buf);
            return false;
        }

        ks_us.Buffer = buf;
    }

    return true;
}

extern "C"
NTSTATUS user_NtDisplayString(const UNICODE_STRING* string) {
    UNICODE_STRING us;

    if (!copy_unicode_string_from_process(string, us))
        return nt_status::ACCESS_VIOLATION;

    auto ret = NtDisplayString(&us);

    if (us.Buffer)
        free(us.Buffer);

    return ret;
}

extern "C"
NTSTATUS user_NtTerminateThread(HANDLE thread_handle, NTSTATUS exit_status) {
    // FIXME - check not kernel handle

    return NtTerminateThread(thread_handle, exit_status);
}
