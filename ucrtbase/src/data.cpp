#include <string_view>
#include <span>
#include <utility>
#include <stdlib.h>

import ucrtbase;
import cmdline;
import nt;
import print;
import unicode;

using namespace std;

static int fmode = _O_TEXT;
static unsigned int commode = 0;
static int argc = 0;
static char** argv = nullptr;
static char** __environ = nullptr;
static int app_type;

extern "C"
void _set_app_type(_crt_app_type type) {
    app_type = type;
}

extern "C"
int* __p___argc() {
    return &argc;
}

extern "C"
char*** __p___argv() {
    return &argv;
}

extern "C"
unsigned int* __p__commode() {
    return &commode;
}

extern "C"
char*** __p__environ() {
    return &__environ;
}

extern "C"
int* __p__fmode() {
    return &fmode;
}

extern "C"
void _initterm(_PVFV* first, _PVFV* last) {
    for (auto* f = first; f < last; f++) {
        if (!*f)
            continue;

        (**f)();
    }
}

extern "C"
int _initterm_e(_PIFV* first, _PIFV* last) {
    for (auto* f = first; f < last; f++) {
        if (!*f)
            continue;

        auto ret = (**f)();
        if (ret != 0)
            return ret;
    }

    return 0;
}

extern "C"
int _initialize_narrow_environment() {
    return 0;
}

static void do_cmdline() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    auto cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                  peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));

    argc = count_cmdline_args(cmdline);

    span<u16string_view> u16argv;
    u16string_view* buf = nullptr;

    if (argc == 0)
        return;

    buf = (decltype(buf))malloc(argc * sizeof(*buf));
    if (!buf) {
        print_stderr("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    argv = (decltype(argv))malloc(argc * sizeof(*argv));
    if (!argv) {
        print_stderr("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    u16argv = span(buf, argc);
    parse_cmdline(cmdline, u16argv);

    for (int i = 0; i < argc; i++) {
        auto arg = u16argv[i];
        auto u8len = utf16_to_utf8_len(arg);

        argv[i] = (char*)malloc(u8len + 1);
        if (!argv[i]) {
            print_stderr("malloc failed\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        utf16_to_utf8(span(argv[i], u8len), arg);
        argv[i][u8len] = 0;
    }

    free(buf);
}

static void do_env() {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    auto env = u16string_view((char16_t*)peb->ProcessParameters->Environment,
                              peb->ProcessParameters->EnvironmentSize / sizeof(char16_t));

    auto env2 = env;
    unsigned int env_size = 0;

    while (true) {
        if (env2.empty() || env2[0] == 0)
            break;

        env_size++;

        auto zero = env2.find(u'\0');

        if (zero != u16string_view::npos)
            env2 = u16string_view(env2.data() + zero + 1, env2.size() - zero - 1);
        else
            break;
    }

    __environ = (char**)malloc((env_size + 1) * sizeof(char*));
    if (!__environ) {
        print_stderr("malloc failed\n");
        NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
        unreachable();
    }

    auto p = __environ;

    while (true) {
        if (env.empty() || env[0] == 0)
            break;

        auto zero = env.find(u'\0');

        u16string_view bit;

        if (zero == u16string_view::npos)
            bit = env;
        else
            bit = u16string_view(env.data(), zero);

        auto u8len = utf16_to_utf8_len(bit);

        *p = (char*)malloc(u8len + 1);
        if (!*p) {
            print_stderr("malloc failed\n");
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);
            unreachable();
        }

        utf16_to_utf8(span(*p, u8len), bit);
        (*p)[u8len] = 0;

        p++;

        if (zero != u16string_view::npos)
            env = u16string_view(env.data() + zero + 1, env.size() - zero - 1);
        else
            break;
    }

    *p = nullptr;
}

extern "C"
int _configure_narrow_argv(int /*mode*/) {
    // FIXME - what is the parameter supposed to do?

    do_cmdline();
    do_env();

    return 0;
}
