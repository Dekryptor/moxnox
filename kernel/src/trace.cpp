module;

#include <stddef.h>
#include <stdint.h>
#include <span>
#include <array>
#include <algorithm>

export module trace;

using namespace std;

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

#ifdef __x86_64__
template<fixed_string str, size_t N>
constexpr void asm_call_trace(span<char, N>& s) {
    s[0] = 'c';
    s[1] = 'a';
    s[2] = 'l';
    s[3] = 'l';
    s[4] = ' ';
    s[5] = 't';
    s[6] = 'r';
    s[7] = 'a';
    s[8] = 'c';
    s[9] = 'e';
    s[10] = '_';

    for (size_t i = 0; i < str.length; i++) {
        s[11 + i] = str.data[i];
    }

    s[11 + str.length] = '\n';

    s = s.last(s.size() - str.length - 12);
}

template<fixed_string str, size_t N>
constexpr void asm_jump(span<char, N>& s) {
    s[0] = 'j';
    s[1] = 'm';
    s[2] = 'p';
    s[3] = ' ';

    for (size_t i = 0; i < str.length; i++) {
        s[4 + i] = str.data[i];
    }

    s[4 + str.length] = '\n';

    s = s.last(s.size() - str.length - 5);
}
#endif

export
template<fixed_string name, size_t N>
consteval auto trace_asm() {
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
    asm_line<".rva .Ltp_handler%=">(s);
    asm_line<".text">(s);
    asm_line<".Ltp_addr%=:">(s);

#ifdef __x86_64__
    asm_line<".byte 0x0f,0x1f,0x44,0x00,0x00">(s);
#elifdef __aarch64__
    asm_line<"nop">(s);
#endif

    asm_line<".section .text$zzz,\"rx\"">(s);
    asm_line<".Ltp_handler%=:">(s);
#ifdef __x86_64__
    asm_line<"push rax">(s);
    asm_line<"push rcx">(s);
    asm_line<"push rdx">(s);
    asm_line<"push r8">(s);
    asm_line<"push r9">(s);
    asm_line<"push r10">(s);
    asm_line<"push r11">(s);
    asm_line<"pushfq">(s);

    // FIXME - args

    // align stack
    asm_line<"push rbp">(s);
    asm_line<"mov rbp, rsp">(s);
    asm_line<"and rsp, 0xfffffffffffffff0">(s);
    asm_line<"sub rsp, 0x20">(s);

    asm_call_trace<name>(s);
    asm_line<"mov rsp, rbp">(s);
    asm_line<"pop rbp">(s);

    asm_line<"popfq">(s);
    asm_line<"pop r11">(s);
    asm_line<"pop r10">(s);
    asm_line<"pop r9">(s);
    asm_line<"pop r8">(s);
    asm_line<"pop rdx">(s);
    asm_line<"pop rcx">(s);
    asm_line<"pop rax">(s);

    asm_jump<".Ltp_addr%= + 5">(s);
#elifdef __aarch64__
    // FIXME
#endif

    asm_line<".text">(s);

    return arr;
}

export bool create_dev_tracectl();
export bool is_trace_point(uintptr_t addr);
