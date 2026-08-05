module;

#include <stddef.h>
#include <stdint.h>
#include <span>
#include <array>
#include <algorithm>

export module trace;

import nt;
import chardev;
import print;
import handle;
import object_ref;

using namespace std;

class tracectl : public chardev {
public:
    tracectl() {
        exclusive = false;
    }

    nt_status read(span<const span<uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }

    nt_status write(span<const span<const uint8_t>>, uint64_t&) override {
        return nt_status::NOT_SUPPORTED;
    }
};

template<size_t N>
struct fixed_string {
    char data[N]{};
    constexpr fixed_string(const char (&str)[N]) {
        copy(str, str + N, data);
    }

    static constexpr size_t length = N - 1;
};

template<size_t N>
constexpr void asm_string_start(span<char, N>& s) {
    s[0] = '.';
    s[1] = 'a';
    s[2] = 's';
    s[3] = 'c';
    s[4] = 'i';
    s[5] = 'z';
    s[6] = ' ';
    s[7] = '"';

    s = s.last(s.size() - 8);
}

template<size_t N>
constexpr void asm_string_end(span<char, N>& s) {
    s[0] = '"';
    s[1] = '\n';

    s = s.last(s.size() - 2);
}

template<fixed_string name, size_t N>
constexpr void asm_string(span<char, N>& s) {
    asm_string_start(s);

    for (size_t i = 0; i < name.length; i++) {
        s[i] = name.data[i];
    }

    s = s.last(s.size() - name.length);

    asm_string_end(s);
}

template<fixed_string str, size_t N>
constexpr void asm_line(span<char, N>& s) {
    for (size_t i = 0; i < str.length; i++) {
        s[i] = str.data[i];
    }

    s[str.length] = '\n';

    s = s.last(s.size() - str.length - 1);
}

export
template<fixed_string name, size_t N>
constexpr auto trace_asm() {
    array<char, 1024> arr{};
    span<char> s(arr);

    asm_line<".section .rdata,\"dr\"">(s);
    asm_line<".Ltp_name%=:">(s);
    asm_string<name>(s);
    asm_line<".Ltp_args%=:">(s);

    asm_string_start(s);
    for (size_t i = 0; i < N; i++) {
        if (i > 0) {
            s[0] = ',';
            s = s.last(s.size() - 1);
        }

        s[0] = '%';

        if (i < 10) {
            s[1] = (char)(i + '0');
            s = s.last(s.size() - 2);
        } else {
            s[1] = (char)((i / 10) + '0');
            s[2] = (char)((i % 10) + '0');
            s = s.last(s.size() - 3);
        }
    }
    asm_string_end(s);

    asm_line<".section .trace,\"dr\"">(s);
    asm_line<"992:">(s);
    asm_line<".rva .Ltp_name%=">(s);
    asm_line<".rva .Ltp_args%=">(s);
    asm_line<".rva .Ltp_addr%=">(s);
    asm_line<".text">(s);
    asm_line<".Ltp_addr%=:">(s);

#ifdef __x86_64__
    asm_line<".byte 0x0f,0x1f,0x44,0x00,0x00">(s);
#elifdef __aarch64__
    asm_line<"nop">(s);
#endif

    return arr;
}

export bool create_dev_tracectl() {
    auto pptr = new tracectl;
    if (!pptr) {
        print("new failed\n");
        return false;
    }

    auto p = object_ref{*pptr};

    auto status = name_object(p.get(), "tracectl");
    if (!success(status)) {
        print("name_object returned {}\n", status);
        return false;
    }

    return true;
}
