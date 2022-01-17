#include <stdint.h>
#include <nt.h>
#include <string_view>
#include <span>

using namespace std;

static void print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardError, 0, nullptr, nullptr, &iosb, s.data(),
                (uint32_t)s.size(), nullptr, nullptr);
}

span<char> sp_itoa_hex(span<char> dest, uint64_t num, unsigned int padding) {
    bool first = true;

    if (dest.empty())
        return dest;

    if (num == 0 && padding == 0) {
        dest[0] = '0';
        return dest.subspan(1);
    }

    for (unsigned int i = 0; i < 16; i++) {
        if (!first || padding == 16 - i || num & 0xf000000000000000) {
            uint64_t n = num >> 60;

            if (n <= 9)
                dest[0] = (char)(n + '0');
            else
                dest[0] = (char)(n - 0xa + 'a');

            dest = dest.subspan(1);
            first = false;

            if (dest.empty())
                return dest;
        }

        num <<= 4;
    }

    return dest;
}

static void print_hex(uint64_t num, unsigned int padding = 0) {
    char s[17];
    span sp(s, sizeof(s));

    sp = sp_itoa_hex(sp, num, padding);

    print(string_view(s, sp.data() - s));
}

extern "C"
size_t strlen(const char* s) {
    size_t i = 0;

    while (s[i] != 0) {
        i++;
    }

    return i;
}

extern "C"
void startup() {
    static const char16_t msg[] = u"hello, world\n";

    UNICODE_STRING us;
    us.Length = us.MaximumLength = sizeof(msg) - sizeof(char16_t);
    us.Buffer = (PWSTR)msg;

    NtDisplayString(&us);

    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;

    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardOutput, 0, nullptr, nullptr, &iosb, "hello\n", 6, nullptr, nullptr);

    while (true) {
        char s[10];

        auto status = NtReadFile(peb->ProcessParameters->StandardInput, 0, nullptr, nullptr, &iosb,
                                 s, sizeof(s), nullptr, nullptr);

        if (!success(status)) {
            print("NtReadFile returned ");
            print_hex((uint32_t)status, 8);
            print("\n");
            break;
        }
    }

    NtTerminateThread(NtCurrentThread(), (NTSTATUS)42);
}
