#include <span>
#include <string_view>
#include <utility>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>

import nt;
import ntdll;
import print;
import unique_handle;
import unicode;
import moxnox_ioctls;

using namespace std;

static const char16_t vfsctl_path[] = u"/dev/vfsctl";

static bool force = false;

static int umount(u16string_view path) {
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
        return 1;
    } else if (status == nt_status::NOT_A_DIRECTORY) {
        print_stderr("umount: {}: Not a directory\n", path);
        return 1;
    } else if (!success(status)) {
        print_stderr("umount: NtCreateFile returned {} for {}\n", status, path);
        return 1;
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
        return 1;
    }

    // get proper name

    status = NtQueryInformationFile(dir.get(), &iosb, &fni, sizeof(fni),
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        print_stderr("umount: {}: NtQueryInformationFile returned {}\n",
                     path, status);
        return 1;
    }

    size_t len = offsetof(FILE_NAME_INFORMATION, FileName) + fni.FileNameLength;

    auto fni2 = (FILE_NAME_INFORMATION*)malloc(len);
    if (!fni2) {
        print_stderr("malloc failed\n");
        return 1;
    }

    status = NtQueryInformationFile(dir.get(), &iosb, fni2, (uint32_t)len,
                                    FILE_INFORMATION_CLASS::NameInformation);
    if (!success(status)) {
        print_stderr("umount: {}: NtQueryInformationFile returned {}\n",
                     path, status);
        free(fni2);
        return 1;
    }

    auto u16sv = u16string_view(fni2->FileName, fni2->FileNameLength / sizeof(char16_t));

    auto u8len = utf16_to_utf8_len(u16sv);
    size_t params_len = offsetof(MOXNOX_VFSCTL_UNMOUNT, path) + u8len;

    auto params = (MOXNOX_VFSCTL_UNMOUNT*)malloc(params_len);
    if (!params) {
        print_stderr("malloc failed\n");
        free(fni2);
        return 1;
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

    return success(status) ? 0 : 1;
}

int main(int argc, char** argv) {
    static const option options[] = {
        { "force", no_argument, nullptr, 'f' },
        { nullptr, 0, nullptr, 0 }
    };

    while (true) {
        int index;
        auto c = getopt_long(argc, argv, "f", options, &index);

        if (c == -1)
            break;

        switch (c) {
            case 'f':
                force = true;
                break;

            case '?':
                return 1;
        }
    }

    if (argc == optind) {
        print_stderr("umount: bad usage\n");
        return 1;
    }

    auto arr = (u16string_view*)malloc((unsigned int)(argc - optind) * sizeof(u16string_view));
    if (!arr) {
        print_stderr("malloc failed\n");
        return 1;
    }

    for (int i = optind; i < argc; i++) {
        auto sv = string_view(argv[i]);

        auto u16len = utf8_to_utf16_len(sv);
        auto u16src = (char16_t*)malloc(u16len * sizeof(char16_t));
        if (!u16src) {
            print_stderr("malloc failed\n");

            for (int j = optind; j < i; j++) {
                free((void*)arr[j - optind].data());
            }
            free(arr);

            return 1;
        }

        utf8_to_utf16(span(u16src, u16len), sv);

        arr[i - optind] = u16string_view(u16src, u16len);
    }

    int ret = 0;

    for (int i = 0; i < argc - optind; i++) {
        if (umount(arr[i]))
            ret = 1;
    }

    for (int i = 0; i < argc - optind; i++) {
        free((void*)arr[i].data());
    }
    free(arr);

    return ret;
}
