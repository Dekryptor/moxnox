#include <span>
#include <string_view>
#include <string.h>
#include <utility>

import nt;
import ntdll;
import cmdline;
import print;
import getopt;
import crt;

using namespace std;

static u16string_view mount_type;

static nt_status mount(u16string_view source, u16string_view directory) {
    // FIXME

    print("mount_type = \"{}\", source = \"{}\", directory = \"{}\"\n",
          mount_type, source, directory);

    // FIXME - if mount_type not given, try to guess (use libblkid?)

    // FIXME - open source
    // FIXME - open directory
    // FIXME - open /dev/vfsctl
    // FIXME - call IOCTL_MOXNOX_VFSCTL_MOUNT

    return nt_status::NOT_IMPLEMENTED;
}

#ifdef __x86_64__
#define FORCE_ALIGN_ARG_POINTER __attribute__((force_align_arg_pointer))
#else
#define FORCE_ALIGN_ARG_POINTER
#endif

extern "C" FORCE_ALIGN_ARG_POINTER void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    nt_status status;

    static const option options[] = {
        { u"type", u't', [](u16string_view param){ mount_type = param; } }, // FIXME - on Linux is actually --types
    };

    auto cmdline = u16string_view(peb->ProcessParameters->CommandLine.Buffer,
                                  peb->ProcessParameters->CommandLine.Length / sizeof(char16_t));

    auto argc = count_cmdline_args(cmdline);
    span<u16string_view> argv;
    u16string_view* buf = nullptr;

    if (argc != 0) {
        buf = (decltype(buf))RtlAllocateHeap(GetProcessHeap(), 0, argc * sizeof(*buf));
        if (!buf)
            NtTerminateThread(NtCurrentThread(), nt_status::INSUFFICIENT_RESOURCES);

        argv = span(buf, argc);
    }

    parse_cmdline(cmdline, argv);

    if (!argv.empty()) // skip program name
        argv = span(argv.data() + 1, argv.size() - 1);

    if (!getopt(argv, options)) {
        if (buf)
            RtlFreeHeap(GetProcessHeap(), 0, buf);

        NtTerminateThread(NtCurrentThread(), nt_status::INVALID_PARAMETER);
        unreachable();
    }

    // FIXME - show list of mounts if no source or directory given
    // FIXME - if only one option, try to mount from /etc/fstab

    if (argv.size() < 2) {
        print_stderr("no source or directory given\n");
        status = nt_status::INVALID_PARAMETER;
    } else
        status = mount(argv[0], argv[1]);

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), status);
}
