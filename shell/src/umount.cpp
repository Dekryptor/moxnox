#include <span>
#include <string_view>
#include <utility>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

import nt;
import ntdll;
import cmdline;
import print;
import unique_handle;
import unicode;
import moxnox_ioctls;
import getopt;

using namespace std;

static const char16_t vfsctl_path[] = u"/dev/vfsctl";

static bool force = false;

static nt_status umount(u16string_view path) {
    auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;
    NTSTATUS status;
    HANDLE h;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    unique_handle dir, vfsctl;
    FILE_NAME_INFORMATION fni;

    // FIXME - also work by specifying block device instead (search through mount list for match)

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = peb->ProcessParameters->CurrentDirectory.Handle;

    // open path

    us.Length = us.MaximumLength = (uint16_t)(path.size() * sizeof(char16_t));
    us.Buffer = (char16_t*)path.data();

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (status == nt_status::OBJECT_NAME_NOT_FOUND ||
        status == nt_status::OBJECT_PATH_NOT_FOUND) {
        print_stderr("umount: {}: No such file or directory\n", path);
        return status;
    } else if (status == nt_status::NOT_A_DIRECTORY) {
        print_stderr("umount: {}: Not a directory\n", path);
        return status;
    } else if (!success(status)) {
        print_stderr("umount: NtCreateFile returned {} for {}\n", status, path);
        return status;
    }

    dir.reset(h);

    // open /dev/vfsctl

    us.Length = us.MaximumLength = (uint16_t)(sizeof(vfsctl_path) - sizeof(char16_t));
    us.Buffer = (char16_t*)vfsctl_path;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                          FILE_OPEN, 0, nullptr, 0);
    if (success(status))
        vfsctl.reset(h);
    else {
        print_stderr("umount: NtCreateFile returned {} for /dev/vfsctl\n", status);
        return status;
    }

    // get proper name

    status = NtQueryInformationFile(dir.get(), &iosb, &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        print_stderr("umount: {}: NtQueryInformationFile returned {}\n",
                     path, status);
        return status;
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        print_stderr("malloc failed\n");
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    status = NtQueryInformationFile(dir.get(), &iosb, fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        print_stderr("umount: {}: NtQueryInformationFile returned {}\n",
                     path, status);
        free(fni2);
        return status;
    }

    auto u16sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

    auto u8len = utf16_to_utf8_len(u16sv);
    size_t params_len = offsetof(MOXNOX_VFSCTL_UNMOUNT, path) + u8len;

    auto params = (MOXNOX_VFSCTL_UNMOUNT*)malloc(params_len);
    if (!params) {
        print_stderr("malloc failed\n");
        free(fni2);
        return nt_status::INSUFFICIENT_RESOURCES;
    }

    params->flags = 0;

    if (force)
        params->flags |= MOXNOX_VFSCTL_UNMOUNT_FLAGS_FORCE;

    params->path_len = u8len;
    utf16_to_utf8(span(params->path, u8len), u16sv);

    free(fni2);

    dir.reset(); // unmount will fail if HANDLE is open

    // ioctl

    // FIXME - check that this is definitely a mount point first, so we don't get any surprises

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_UNMOUNT, params,
                                   (uint32_t)params_len, nullptr, 0);
    if (!success(status))
        print("umount: {}: IOCTL_MOXNOX_VFSCTL_UNMOUNT returned {}\n", path, status);

    free(params);

    return status;
}

extern "C" void startup() {
    auto teb = NtCurrentTeb();
    auto peb = teb->ProcessEnvironmentBlock;
    nt_status status;

    static const option options[] = {
        { u"force", u'f', []{ force = true; } },
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

    if (argv.size() == 0) {
        print_stderr("umount: bad usage\n");
        status = nt_status::INVALID_PARAMETER;
    }

    status = nt_status::SUCCESS;

    for (auto arg : argv) {
        if (auto status2 = umount(arg); !success(status2))
            status = status2;
    }

    if (buf)
        RtlFreeHeap(GetProcessHeap(), 0, buf);

    NtTerminateThread(NtCurrentThread(), status);
}
