#include <span>
#include <string_view>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <utility>

import nt;
import ntdll;
import print;
import unique_handle;
import unicode;
import moxnox_ioctls;

using namespace std;

static const char16_t vfsctl_path[] = u"/dev/vfsctl";

static char* mount_type = nullptr;

static int mount(string_view source, string_view directory) {
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

    if (source != "none") {
        auto u16len = utf8_to_utf16_len(source);
        auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));
        if (!u16) {
            print_stderr("malloc failed\n");
            return 1;
        }

        utf8_to_utf16(span(u16, u16len), source);

        us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
        us.Buffer = u16;

        status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr,
                              0, 0, FILE_OPEN, FILE_NON_DIRECTORY_FILE,
                              nullptr, 0);

        free(u16);

        if (success(status))
            src.reset(h);
        else {
            print_stderr("mount: NtCreateFile returned {} for {}\n", status,
                         source);
            return 1;
        }
    }

    // open directory

    {
        auto u16len = utf8_to_utf16_len(directory);
        auto u16 = (char16_t*)malloc(u16len * sizeof(char16_t));
        if (!u16) {
            print_stderr("malloc failed\n");
            return 1;
        }

        utf8_to_utf16(span(u16, u16len), directory);

        us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
        us.Buffer = u16;

        status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr, 0, 0,
                            FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);

        free(u16);

        if (success(status))
            dir.reset(h);
        else {
            print_stderr("mount: NtCreateFile returned {} for {}\n", status, directory);
            return 1;
        }
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
        return 1;
    }

    string_view type;

    if (mount_type)
        type = mount_type;

    size_t buflen = sizeof(MOXNOX_VFSCTL_MOUNT) + type.size();
    auto buf = (MOXNOX_VFSCTL_MOUNT*)malloc(buflen);
    if (!buf) {
        print_stderr("malloc failed\n");
        return 1;
    }

    if (!type.empty())
        buf->mount_type = sizeof(MOXNOX_VFSCTL_MOUNT);
    else
        buf->mount_type = 0;

    buf->source = src ? src.get() : 0;
    buf->directory = dir.get();

    if (!type.empty())
        memcpy((char*)buf + buf->mount_type, type.data(), type.size());

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_MOUNT, buf,
                                   (uint32_t)buflen, nullptr, 0);
    if (!success(status))
        print("IOCTL_MOXNOX_VFSCTL_MOUNT returned {}\n", status);

    free(buf);

    return success(status) ? 0 : 1;
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

    if (source.empty())
        source = type;

    print("{} on {} type {}\n", source, directory, type);
}

static int list_mounts() {
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
        return 1;
    }

    vfsctl.reset(h);

    // get mount IDs

    MOXNOX_VFSCTL_LIST_MOUNTS lm1;

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS, nullptr, 0,
                                   &lm1, offsetof(MOXNOX_VFSCTL_LIST_MOUNTS, ids));
    if (!success(status) && status != nt_status::BUFFER_OVERFLOW) {
        print("IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS returned {}\n", status);
        return 1;
    }

    size_t lm_size = offsetof(MOXNOX_VFSCTL_LIST_MOUNTS, ids) + (lm1.num * sizeof(uint64_t));
    auto lm2 = (MOXNOX_VFSCTL_LIST_MOUNTS*)malloc(lm_size);
    if (!lm2) {
        print("malloc failed\n");
        return 1;
    }

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS, nullptr, 0,
                                   lm2, (uint32_t)lm_size);
    if (!success(status)) {
        print("IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS returned {}\n", status);
        free(lm2);
        return 1;
    }

    // FIXME - really ought to start from the beginning again if get_mount_info encounters nt_status::NOT_FOUND

    for (unsigned int i = 0; i < lm2->num; i++) {
        get_mount_info(vfsctl.get(), lm2->ids[i]);
    }

    free(lm2);

    return 0;
}

int main(int argc, char** argv) {
    int ret;

    static const option options[] = {
        { "type", required_argument, nullptr, 't' }, // FIXME - on Linux is actually --types
        { nullptr, 0, nullptr, 0 }
    };

    while (true) {
        int index;
        auto c = getopt_long(argc, argv, "t:", options, &index);

        if (c == -1)
            break;

        switch (c) {
            case 't':
                mount_type = strdup(optarg);
                if (!mount_type) {
                    print_stderr("strdup failed\n");
                    return 1;
                }
                break;

            case '?':
                return 1;
        }
    }

    // FIXME - if only one option, try to mount from /etc/fstab

    if (argc == optind)
        ret = list_mounts();
    else if (argc == optind + 1) {
        print_stderr("no source or directory given\n");
        ret = 1;
    } else
        ret = mount(argv[optind], argv[optind + 1]);

    if (mount_type)
        free(mount_type);

    return ret;
}
