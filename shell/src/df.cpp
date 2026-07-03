#include <span>
#include <string_view>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <getopt.h>
#include <utility>
#include <memory>

import nt;
import ntdll;
import print;
import crt;
import unique_handle;
import unicode;
import moxnox_ioctls;

using namespace std;

static const char16_t vfsctl_path[] = u"/dev/vfsctl";

static void get_mount_info(HANDLE vfsctl, uint64_t num) {
    IO_STATUS_BLOCK iosb;
    nt_status status;
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    unique_ptr<char16_t[]> u16;
    HANDLE h;
    unique_handle f;
    FILE_FS_SIZE_INFORMATION fs_info;
    char buf[512];

    auto& info = *(MOXNOX_VFSCTL_MOUNT_INFO*)buf;

    status = NtDeviceIoControlFile(vfsctl, 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_GET_MOUNT_INFO,
                                   &num, sizeof(num), &info, (uint32_t)sizeof(buf));
    if (!success(status)) {
        print_stderr("IOCTL_MOXNOX_VFSCTL_GET_MOUNT_INFO returned {}\n", status);
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

    // FIXME - Filesystem, 1K-blocks, Used, Available, Use%, Mounted on

    auto u16len = utf8_to_utf16_len(directory);
    u16.reset(new char16_t[u16len]);
    if (!u16) {
        print_stderr("new failed\n");
        return;
    }

    utf8_to_utf16(span(u16.get(), u16len), directory);

    us.Length = us.MaximumLength = (uint16_t)(u16len * sizeof(char16_t));
    us.Buffer = u16.get();

    memset(&oa, 0, sizeof(oa));
    oa.Length = sizeof(OBJECT_ATTRIBUTES);
    oa.ObjectName = &us;
    oa.RootDirectory = 0;

    status = NtCreateFile(&h, FILE_READ_ATTRIBUTES, &oa, &iosb, nullptr,
                          0, 0, FILE_OPEN, FILE_DIRECTORY_FILE, nullptr, 0);
    if (!success(status)) {
        print_stderr("NtCreateFile returned {} for {}\n", status, directory);
        return;
    } else
        f.reset(h);

    status = NtQueryVolumeInformationFile(f.get(), &iosb, &fs_info, sizeof(fs_info),
                                          FS_INFORMATION_CLASS::SizeInformation);
    if (!success(status)) {
        print_stderr("NtQueryVolumeInformationFile returned {} for {}\n", status, directory);
        return;
    }

    uint64_t cluster_size = fs_info.BytesPerSector * fs_info.SectorsPerAllocationUnit;

    uint64_t total_size = (uint64_t)fs_info.TotalAllocationUnits.QuadPart * cluster_size;
    uint64_t used_size = ((uint64_t)fs_info.TotalAllocationUnits.QuadPart - (uint64_t)fs_info.AvailableAllocationUnits.QuadPart) * cluster_size;

    print("{}\t{}\t{}\t{}\t{}%\t{}\n",
          source, total_size / 1024, used_size / 1024, (total_size - used_size) / 1024,
          used_size * 100 / total_size, directory);
}

static int list_all(bool human_readable) {
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
        print_stderr("IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS returned {}\n", status);
        return 1;
    }

    size_t lm_size = offsetof(MOXNOX_VFSCTL_LIST_MOUNTS, ids) + (lm1.num * sizeof(uint64_t));
    auto lm2 = (MOXNOX_VFSCTL_LIST_MOUNTS*)malloc(lm_size);
    if (!lm2) {
        print_stderr("malloc failed\n");
        return 1;
    }

    status = NtDeviceIoControlFile(vfsctl.get(), 0, nullptr, nullptr, &iosb,
                                   IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS, nullptr, 0,
                                   lm2, (uint32_t)lm_size);
    if (!success(status)) {
        print_stderr("IOCTL_MOXNOX_VFSCTL_LIST_MOUNTS returned {}\n", status);
        free(lm2);
        return 1;
    }

    // FIXME - proper alignment

    print("Filesystem\t1K-blocks\tUsed\tAvailable\tUse%\tMounted on\n");

    for (unsigned int i = 0; i < lm2->num; i++) {
        get_mount_info(vfsctl.get(), lm2->ids[i]);
    }

    free(lm2);

    return 0;
}

int main(int argc, char** argv) {
    bool human_readable = false;

    static const option options[] = {
        { "human-readable", no_argument, nullptr, 'h' },
        { nullptr, 0, nullptr, 0 }
    };

    while (true) {
        int index;
        auto c = getopt_long(argc, argv, "h", options, &index);

        if (c == -1)
            break;

        switch (c) {
            case 't':
                human_readable = true;
                break;

            case '?':
                return 1;
        }
    }

    // FIXME - querying specific directory

    return list_all(human_readable);
}
