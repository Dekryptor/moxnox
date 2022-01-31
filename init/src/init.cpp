#include <stdint.h>
#include <stdlib.h>
#include <nt.h>
#include <ntdll.h>
#include <string_view>
#include <span>

using namespace std;

char* buf = nullptr;
unsigned int buflen = 0;

static void print(string_view s) {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    IO_STATUS_BLOCK iosb;

    NtWriteFile(peb->ProcessParameters->StandardOutput, 0, nullptr, nullptr, &iosb, s.data(),
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
void* malloc(size_t size) {
    return RtlAllocateHeap(GetProcessHeap(), 0, size);
}

extern "C"
void free(void* ptr) {
    RtlFreeHeap(GetProcessHeap(), 0, ptr);
}

static void process_line(string_view sv) {
    // FIXME

    print("line: \"");
    print(sv);
    print("\"\n");
}

static void process_lines() {
    while (true) {
        bool found = false;
        auto sv = string_view(buf, buflen);

        for (unsigned int i = 0; i < sv.size(); i++) {
            if (sv[i] == '\n') {
                process_line(string_view(sv.data(), i));

                auto newsv = string_view(sv.data() + i + 1, sv.size() - i - 1);

                if (newsv.size() > 0) {
                    auto buf2 = (char*)malloc(newsv.size());
                    if (!buf2) {
                        print("out of memory\n");
                        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
                    }

                    memcpy(buf2, newsv.data(), newsv.size());
                    free(buf);
                    buf = buf2;
                } else {
                    free(buf);
                    buf = nullptr;
                }

                buflen = (unsigned int)newsv.size();

                found = true;
                break;
            }
        }

        if (!found)
            return;
    }
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
            print("NtReadFile returned "); // FIXME - should be to stderr
            print_hex((uint32_t)status, 8);
            print("\n");
            break;
        }

        if (iosb.Information == 0)
            continue;

        auto sv = string_view(s, iosb.Information);

        print(sv);

        auto buf2 = (char*)malloc(buflen + iosb.Information);
        if (!buf2) {
            print("out of memory\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        }

        if (buf) {
            memcpy(buf2, buf, buflen);
            free(buf);
        }

        memcpy(buf2 + buflen, s, iosb.Information);
        buflen += (unsigned int)iosb.Information;
        buf = buf2;

        bool nl = false;
        for (auto c : sv) {
            if (c == '\n') {
                nl = true;
                break;
            }
        }

        if (nl)
            process_lines();
    }

    NtTerminateThread(NtCurrentThread(), nt_status::SUCCESS);
}
