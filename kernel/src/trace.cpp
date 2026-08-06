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

constexpr char hex_digit(unsigned int num) {
    if (num < 10)
        return (char)('0' + num);
    else
        return (char)('a' + num - 10);
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

template<size_t N>
constexpr void asm_push_param(span<char, N>& s, unsigned int num) {
    s[0] = 'p';
    s[1] = 'u';
    s[2] = 's';
    s[3] = 'h';
    s[4] = ' ';
    s[5] = '%';
    s[6] = 'q';

    if (num < 10) {
        s[7] = (char)(num + '0');
        s[8] = '\n';
        s = s.last(s.size() - 9);
    } else {
        s[7] = (char)((num / 10) + '0');
        s[8] = (char)((num % 10) + '0');
        s[9] = '\n';
        s = s.last(s.size() - 10);
    }
}

template<size_t N>
constexpr void asm_add_rsp(span<char, N>& s, unsigned int size) {
    if (size == 0)
        return;

    s[0] = 'a';
    s[1] = 'd';
    s[2] = 'd';
    s[3] = ' ';
    s[4] = 'r';
    s[5] = 's';
    s[6] = 'p';
    s[7] = ',';
    s[8] = '0';
    s[9] = 'x';
    s[10] = hex_digit((size >> 4) & 0xf);
    s[11] = hex_digit(size & 0xf);
    s[12] = '\n';

    s = s.last(s.size() - 13);
}

#elifdef __aarch64__

template<fixed_string str, size_t N>
constexpr void asm_call_trace(span<char, N>& s) {
    s[0] = 'b';
    s[1] = 'l';
    s[2] = ' ';
    s[3] = 't';
    s[4] = 'r';
    s[5] = 'a';
    s[6] = 'c';
    s[7] = 'e';
    s[8] = '_';

    for (size_t i = 0; i < str.length; i++) {
        s[9 + i] = str.data[i];
    }

    s[9 + str.length] = '\n';

    s = s.last(s.size() - str.length - 10);
}

template<fixed_string str, size_t N>
constexpr void asm_jump(span<char, N>& s) {
    s[0] = 'b';
    s[1] = ' ';

    for (size_t i = 0; i < str.length; i++) {
        s[2 + i] = str.data[i];
    }

    s[2 + str.length] = '\n';

    s = s.last(s.size() - str.length - 3);
}

template<size_t N>
constexpr void asm_change_sp(span<char, N>& s, int32_t delta) {
    if (delta < 0) {
        s[0] = 's';
        s[1] = 'u';
        s[2] = 'b';
    } else {
        s[0] = 'a';
        s[1] = 'd';
        s[2] = 'd';
    }
    s[3] = ' ';
    s[4] = 's';
    s[5] = 'p';
    s[6] = ',';
    s[7] = ' ';
    s[8] = 's';
    s[9] = 'p';
    s[10] = ',';
    s[11] = ' ';
    s[12] = '#';
    s[13] = '0';
    s[14] = 'x';

    uint32_t abs = delta < 0 ? (uint32_t)-delta : (uint32_t)delta;

    s[15] = hex_digit((abs >> 8) & 0xf);
    s[16] = hex_digit((abs >> 4) & 0xf);
    s[17] = hex_digit(abs & 0xf);
    s[18] = '\n';

    s = s.last(s.size() - 19);
}

template<size_t N>
constexpr void asm_str_param_sp(span<char, N>& s, unsigned int arg, size_t off) {
    s[0] = 's';
    s[1] = 't';
    s[2] = 'r';
    s[3] = ' ';
    s[4] = '%';

    if (arg < 10) {
        s[5] = (char)(arg + '0');
        s = s.last(s.size() - 6);
    } else {
        s[5] = (char)((arg / 10) + '0');
        s[6] = (char)((arg % 10) + '0');
        s = s.last(s.size() - 7);
    }

    s[0] = ',';
    s[1] = ' ';
    s[2] = '[';
    s[3] = 's';
    s[4] = 'p';
    s[5] = ',';
    s[6] = ' ';
    s[7] = '#';
    s[8] = '0';
    s[9] = 'x';

    s[10] = hex_digit((off >> 8) & 0xf);
    s[11] = hex_digit((off >> 4) & 0xf);
    s[12] = hex_digit(off & 0xf);
    s[13] = ']';
    s[14] = '\n';

    s = s.last(s.size() - 15);
}

template<size_t N>
constexpr void asm_stp_x0_x1(span<char, N>& s, size_t off) {
    s[0] = 's';
    s[1] = 't';
    s[2] = 'p';
    s[3] = ' ';
    s[4] = 'x';
    s[5] = '0';
    s[6] = ',';
    s[7] = ' ';
    s[8] = 'x';
    s[9] = '1';
    s[10] = ',';
    s[11] = ' ';
    s[12] = '[';
    s[13] = 's';
    s[14] = 'p';
    s[15] = ',';
    s[16] = ' ';
    s[17] = '#';
    s[18] = '0';
    s[19] = 'x';

    s[20] = hex_digit((off >> 8) & 0xf);
    s[21] = hex_digit((off >> 4) & 0xf);
    s[22] = hex_digit(off & 0xf);
    s[23] = ']';
    s[24] = '\n';

    s = s.last(s.size() - 25);
}

template<size_t N>
constexpr void asm_ldp_x0_x1(span<char, N>& s, size_t off) {
    s[0] = 'l';
    s[1] = 'd';
    s[2] = 'p';
    s[3] = ' ';
    s[4] = 'x';
    s[5] = '0';
    s[6] = ',';
    s[7] = ' ';
    s[8] = 'x';
    s[9] = '1';
    s[10] = ',';
    s[11] = ' ';
    s[12] = '[';
    s[13] = 's';
    s[14] = 'p';
    s[15] = ',';
    s[16] = ' ';
    s[17] = '#';
    s[18] = '0';
    s[19] = 'x';

    s[20] = hex_digit((off >> 8) & 0xf);
    s[21] = hex_digit((off >> 4) & 0xf);
    s[22] = hex_digit(off & 0xf);
    s[23] = ']';
    s[24] = '\n';

    s = s.last(s.size() - 25);
}

#endif

export
template<fixed_string name, size_t N>
consteval auto trace_asm() {
    array<char, 4096> arr{};
    span<char> s(arr);

    asm_line<".section .rdata,\"dr\"">(s);
    asm_line<".Ltp_name%=:">(s);
    asm_string<name>(s);

    asm_line<".section .trace,\"dr\"">(s);
    asm_line<"992:">(s);
    asm_line<".rva .Ltp_name%=">(s);
    asm_line<".rva .Ltp_addr%=">(s);
    asm_line<".rva .Ltp_handler%=">(s);
    asm_line<".text">(s);
    asm_line<".Ltp_addr%=:">(s);

#ifdef __x86_64__
    asm_line<".byte 0x0f,0x1f,0x44,0x00,0x00">(s);
#elifdef __aarch64__
    asm_line<"nop">(s);
#endif

    asm_line<"Ltp_ret%=:">(s);

    asm_line<".section .text$zzz,\"x\"">(s);
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

    for (unsigned int i = 0; i < N; i++) {
        asm_push_param(s, N - i - 1);
    }

    // align stack
    asm_line<"push rbp">(s);
    asm_line<"mov rbp, rsp">(s);
    asm_line<"and rsp, 0xfffffffffffffff0">(s);
    asm_line<"sub rsp, 0x20">(s);

    asm_line<"lea rcx, [rbp + 8]">(s);
    asm_call_trace<name>(s);
    asm_line<"mov rsp, rbp">(s);
    asm_line<"pop rbp">(s);
    asm_add_rsp(s, 8 * N);

    asm_line<"popfq">(s);
    asm_line<"pop r11">(s);
    asm_line<"pop r10">(s);
    asm_line<"pop r9">(s);
    asm_line<"pop r8">(s);
    asm_line<"pop rdx">(s);
    asm_line<"pop rcx">(s);
    asm_line<"pop rax">(s);

    asm_jump<"Ltp_ret%=">(s);
#elifdef __aarch64__
    asm_change_sp(s, -0x200);
    asm_line<"stp q0, q1, [sp]">(s);
    asm_line<"stp q2, q3, [sp, #0x20]">(s);
    asm_line<"stp q4, q5, [sp, #0x40]">(s);
    asm_line<"stp q6, q7, [sp, #0x60]">(s);
    asm_line<"stp q8, q9, [sp, #0x80]">(s);
    asm_line<"stp q10, q11, [sp, #0xa0]">(s);
    asm_line<"stp q12, q13, [sp, #0xc0]">(s);
    asm_line<"stp q14, q15, [sp, #0xe0]">(s);
    asm_line<"stp q16, q17, [sp, #0x100]">(s);
    asm_line<"stp q18, q19, [sp, #0x120]">(s);
    asm_line<"stp q20, q21, [sp, #0x140]">(s);
    asm_line<"stp q22, q23, [sp, #0x160]">(s);
    asm_line<"stp q24, q25, [sp, #0x180]">(s);
    asm_line<"stp q26, q27, [sp, #0x1a0]">(s);
    asm_line<"stp q28, q29, [sp, #0x1c0]">(s);
    asm_line<"stp q30, q31, [sp, #0x1e0]">(s);

    size_t param_bytes = N * 8;

    if (N % 2)
        param_bytes += 8;

    asm_change_sp(s, - (int32_t)(0xa0 + param_bytes));
    asm_line<"stp x0, x1, [sp]">(s);
    asm_line<"stp x2, x3, [sp, #0x10]">(s);
    asm_line<"stp x4, x5, [sp, #0x20]">(s);
    asm_line<"stp x6, x7, [sp, #0x30]">(s);
    asm_line<"stp x8, x9, [sp, #0x40]">(s);
    asm_line<"stp x10, x11, [sp, #0x50]">(s);
    asm_line<"stp x12, x13, [sp, #0x60]">(s);
    asm_line<"stp x14, x15, [sp, #0x70]">(s);
    asm_line<"stp x16, x17, [sp, #0x80]">(s);

    for (unsigned int i = 0; i < N; i++) {
        asm_str_param_sp(s, i, 0x90 + (i * 8));
    }

    asm_line<"mov x0, lr">(s);
    asm_line<"mrs x1, nzcv">(s);
    asm_stp_x0_x1(s, 0x90 + param_bytes);

    asm_line<"add x0, sp, #0x90">(s);
    asm_call_trace<name>(s);

    asm_ldp_x0_x1(s, 0x90 + param_bytes);
    asm_line<"mov lr, x0">(s);
    asm_line<"msr nzcv, x1">(s);

    asm_line<"ldp x16, x17, [sp, #0x80]">(s);
    asm_line<"ldp x14, x15, [sp, #0x70]">(s);
    asm_line<"ldp x12, x13, [sp, #0x60]">(s);
    asm_line<"ldp x10, x11, [sp, #0x50]">(s);
    asm_line<"ldp x8, x9, [sp, #0x40]">(s);
    asm_line<"ldp x6, x7, [sp, #0x30]">(s);
    asm_line<"ldp x4, x5, [sp, #0x20]">(s);
    asm_line<"ldp x2, x3, [sp, #0x10]">(s);
    asm_line<"ldp x0, x1, [sp]">(s);
    asm_change_sp(s, 0xa0 + param_bytes);

    asm_line<"ldp q30, q31, [sp, #0x1e0]">(s);
    asm_line<"ldp q28, q29, [sp, #0x1c0]">(s);
    asm_line<"ldp q26, q27, [sp, #0x1a0]">(s);
    asm_line<"ldp q24, q25, [sp, #0x180]">(s);
    asm_line<"ldp q22, q23, [sp, #0x160]">(s);
    asm_line<"ldp q20, q21, [sp, #0x140]">(s);
    asm_line<"ldp q18, q19, [sp, #0x120]">(s);
    asm_line<"ldp q16, q17, [sp, #0x100]">(s);
    asm_line<"ldp q14, q15, [sp, #0xe0]">(s);
    asm_line<"ldp q12, q13, [sp, #0xc0]">(s);
    asm_line<"ldp q10, q11, [sp, #0xa0]">(s);
    asm_line<"ldp q8, q9, [sp, #0x80]">(s);
    asm_line<"ldp q6, q7, [sp, #0x60]">(s);
    asm_line<"ldp q4, q5, [sp, #0x40]">(s);
    asm_line<"ldp q2, q3, [sp, #0x20]">(s);
    asm_line<"ldp q0, q1, [sp]">(s);
    asm_change_sp(s, 0x200);

    asm_jump<"Ltp_ret%=">(s);
#endif

    asm_line<".text">(s);

    return arr;
}

export bool create_dev_tracectl();
export bool is_trace_point(uintptr_t addr);
