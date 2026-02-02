#include <span>
#include <string_view>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <utility>

import nt;
import ntdll;
import cmdline;
import print;
import getopt;
import crt;
import unique_handle;
import unicode;
import moxnox_ioctls;

using namespace std;

static u16string_view mount_type;

static nt_status mount(u16string_view source, u16string_view directory) {
    NTSTATUS status;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    unique_handle dir, vfsctl;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    static const char16_t vfsctl_path[] = u"/dev/vfsctl";

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    print("mount_type = \"{}\", source = \"{}\", directory = \"{}\"\n",
          mount_type, source, directory);

    // FIXME - if mount_type not given, try to guess (use libblkid?)

    // FIXME - open source
    // open directory

    us.Length = us.MaximumLength = (uint16_t)(directory.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)directory.data();

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (success(status))
        dir.reset(h);
    else {
        print_stderr("mount: NtCreateFile returned {} for {}\n", status, directory);
        return status;
    }

    // open /dev/vfsctl

    us.Length = us.MaximumLength = (uint16_t)(sizeof(vfsctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)vfsctl_path;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_SYNCHRONOUS_IO_ALERT, nullptr, 0);
    if (success(status))
        vfsctl.reset(h);
    else {
        print_stderr("mount: NtCreateFile returned {} for /dev/vfsctl\n", status);
        return status;
    }

    auto u8len = utf16_to_utf8_len(mount_type);
    size_t buflen = sizeof(MOXNOX_VFSCTL_MOUNT) + u8len;
    auto buf = (MOXNOX_VFSCTL_MOUNT*)malloc(buflen);
    if (!buf) {
        print_stderr("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    buf->mount_type = sizeof(MOXNOX_VFSCTL_MOUNT);
    buf->source = 0; // FIXME
    buf->directory = dir.get();

    utf16_to_utf8(span((char*)buf + buf->mount_type, u8len), mount_type);

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_MOUNT, buf,
                                   (uint32_t)buflen, nullptr, 0);
    if (!success(status))
        print("IOCTL_MOXNOX_VFSCTL_MOUNT returned {}\n", status);

    free(buf);

    return status;
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
