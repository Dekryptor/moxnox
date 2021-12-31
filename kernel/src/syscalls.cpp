#include "kernel.h"
#include "nt.h"
#include "objects.h"

static NTSTATUS NtDisplayString(const UNICODE_STRING* string) {
    // FIXME

    print("NtDisplayString\n");

    return nt_status::SUCCESS;
}

static bool copy_from_process(const void* us_addr, void* ks_addr, size_t size) {
    __asm("cli");
    auto t = get_current_thread();
    __asm("sti");

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
