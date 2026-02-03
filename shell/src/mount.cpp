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

static const char16_t vfsctl_path[] = u"/dev/vfsctl";

static u16string_view mount_type;

static nt_status mount(u16string_view source, u16string_view directory) {
    NTSTATUS status;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    unique_handle src, dir, vfsctl;
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    // FIXME - if mount_type not given, try to guess (use libblkid?)

    // open source

    if (source != u"none") {
        us.Length = us.MaximumLength = (uint16_t)(source.size() * sizeof(char16_t));
        us.Buffer = (char16_t*)source.data();

        status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr,
                              0, 0, FILE_OPEN, FILE_NON_DIRECTORY_FILE,
                              nullptr, 0);
        if (success(status))
            src.reset(h);
        else {
            print_stderr("mount: NtCreateFile returned {} for {}\n", status,
                         source);
            return status;
        }
    }

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

    if (!mount_type.empty())
        buf->mount_type = sizeof(MOXNOX_VFSCTL_MOUNT);
    else
        buf->mount_type = 0;

    buf->source = src ? src.get() : 0;
    buf->directory = dir.get();

    if (!mount_type.empty())
        utf16_to_utf8(span((char*)buf + buf->mount_type, u8len), mount_type);

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_MOUNT, buf,
                                   (uint32_t)buflen, nullptr, 0);
    if (!success(status))
        print("IOCTL_MOXNOX_VFSCTL_MOUNT returned {}\n", status);

    free(buf);

    return status;
}

static void get_mount_info(HANDLE vfsctl, uint64_t num) {
    IO_STATUS_BLOCK iosb;
    nt_status status;
    char buf[512];

    auto& info = *(MOXNOX_VFSCTL_MOUNT_INFO*)buf;

    status = NtDeviceIoControlFile(vfsctl, 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_GET_MOUNT_INFO,
                                   &num, sizeof(num), &info, (uint32_t)sizeof(buf));
    if (!success(status)) {
        print("IOCTL_MOXNOX_VFSCTL_GET_MOUNT_INFO returned {}\n", status);
        return;
    }

    string_view source, directory, type;

    if (info.source != 0)
        source = string_view((char*)&info + info.source);

    if (info.directory != 0)
        directory = string_view((char*)&info + info.directory);

    if (info.type != 0)
        type = string_view((char*)&info + info.type);

    print("{} on {} type {}\n", source, directory, type);
}

static nt_status list_mounts() {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    nt_status status;
    HANDLE h;
    unique_handle vfsctl;

    // open /dev/vfsctl

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;

    us.Length = us.MaximumLength = (uint16_t)(sizeof(vfsctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)vfsctl_path;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, 0, nullptr, 0);
    if (!success(status)) {
        print_stderr("mount: NtCreateFile returned {} for /dev/vfsctl\n", status);
        return status;
    }

    vfsctl.reset(h);

    // get mount IDs

    MOXNOX_VFSCTL_LIST_MOUNTS lm1;

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS, nullptr, 0,
                                   &lm1, offsetof(MOXNOX_VFSCTL_LIST_MOUNTS, ids));
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        print("IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS returned {}\n", status);
        return status;
    }

    size_t lm_size = offsetof(MOXNOX_VFSCTL_LIST_MOUNTS, ids) + (lm1.num * sizeof(uint64_t));
    auto lm2 = (MOXNOX_VFSCTL_LIST_MOUNTS*)malloc(lm_size);
    if (!lm2) {
        print("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS, nullptr, 0,
                                   lm2, (uint32_t)lm_size);
    if (!success(status)) {
        print("IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS returned {}\n", status);
        free(lm2);
        return status;
    }

    // FIXME - really ought to start from the beginning again if get_mount_info encounters nt_status::NOT_FOUND

    for (unsigned int i = 0; i < lm2->num; i++) {
        get_mount_info(vfsctl.get(), lm2->ids[i]);
    }

    free(lm2);

    return nt_status::SUCCESS;
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

    // FIXME - if only one option, try to mount from /etc/fstab

    if (argv.size() == 0)
        status = list_mounts();
    else if (argv.size() < 2) {
        print_stderr("no source or directory given\n");
        status = nt_status::INVALID_PARAMETER;
    } else
        status = mount(argv[0], argv[1]);

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), status);
}
