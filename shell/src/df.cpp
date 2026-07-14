#include <span>
#include <string_view>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <getopt.h>
#include <utility>
#include <memory>
#include <algorithm>

import nt;
import ntdll;
import print;
import crt;
import unique_handle;
import unicode;
import moxnox_ioctls;
import format;

using namespace std;

static const char16_t vfsctl_path[] = u"/dev/vfsctl";

struct mount_info {
    char source_buf[PATH_MAX];
    char total_buf[50];
    char used_buf[50];
    char available_buf[50];
    char usedpc_buf[5];
    char directory_buf[PATH_MAX];
    string_view source;
    string_view total;
    string_view used;
    string_view available;
    string_view usedpc;
    string_view directory;
};

static void get_mount_info(HANDLE vfsctl, uint64_t num, mount_info& mi,
                           bool human_readable) {
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

    span source_span = span(mi.source_buf, sizeof(mi.source_buf));
    span total_span = span(mi.total_buf, sizeof(mi.total_buf));
    span used_span = span(mi.used_buf, sizeof(mi.used_buf));
    span available_span = span(mi.available_buf, sizeof(mi.available_buf));
    span usedpc_span = span(mi.usedpc_buf, sizeof(mi.usedpc_buf));
    span directory_span = span(mi.directory_buf, sizeof(mi.directory_buf));

    source_span = format(source_span, "{}", source);

    if (human_readable) {
        total_span = format(total_span, "{}", byte_size(total_size));
        used_span = format(used_span, "{}", byte_size(used_size));
        available_span = format(available_span, "{}", byte_size(total_size - used_size));
    } else {
        total_span = format(total_span, "{}", total_size / 1024);
        used_span = format(used_span, "{}", used_size / 1024);
        available_span = format(available_span, "{}", (total_size - used_size) / 1024);
    }

    usedpc_span = format(usedpc_span, "{}%", used_size * 100 / total_size);
    directory_span = format(directory_span, "{}", directory);

    mi.source = string_view(mi.source_buf, (size_t)(source_span.data() - mi.source_buf));
    mi.total = string_view(mi.total_buf, (size_t)(total_span.data() - mi.total_buf));
    mi.used = string_view(mi.used_buf, (size_t)(used_span.data() - mi.used_buf));
    mi.available = string_view(mi.available_buf, (size_t)(available_span.data() - mi.available_buf));
    mi.usedpc = string_view(mi.usedpc_buf, (size_t)(usedpc_span.data() - mi.usedpc_buf));
    mi.directory = string_view(mi.directory_buf, (size_t)(directory_span.data() - mi.directory_buf));
}

static int list_all(bool human_readable) {
    OBJECT_ATTRIBUTES oa;
    UNICODE_STRING us;
    IO_STATUS_BLOCK iosb;
    nt_status status;
    HANDLE h;
    unique_handle vfsctl;
    unique_ptr<mount_info[]> info;

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

    info.reset(new mount_info[lm2->num + 1]);
    if (!info) {
        print_stderr("new failed\n");
        free(lm2);
        return 1;
    }

    for (unsigned int i = 0; i < lm2->num; i++) {
        get_mount_info(vfsctl.get(), lm2->ids[i], info[i + 1], human_readable);
    }

    info[0].source = "Filesystem";
    info[0].total = "1K-blocks";
    info[0].used = "Used";
    info[0].available = "Available";
    info[0].usedpc = "Use%";
    info[0].directory = "Mounted on";

    size_t source_len = 0, total_len = 0, used_len = 0, available_len = 0, usedpc_len = 0;
    for (unsigned int i = 0; i < lm2->num + 1; i++) {
        source_len = max(source_len, info[i].source.size());
        total_len = max(total_len, info[i].total.size());
        used_len = max(used_len, info[i].used.size());
        available_len = max(available_len, info[i].available.size());
        usedpc_len = max(usedpc_len, info[i].usedpc.size());
    }

    // FIXME - numbers should be right-aligned

    for (unsigned int i = 0; i < lm2->num + 1; i++) {
        auto& mi = info[i];

        print("{:{}} {:>{}} {:>{}} {:>{}} {:>{}} {}\n",
              mi.source, source_len, mi.total, total_len,
              mi.used, used_len, mi.available, available_len,
              mi.usedpc, usedpc_len, mi.directory);
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
            case 'h':
                human_readable = true;
                break;

            case '?':
                return 1;
        }
    }

    // FIXME - querying specific directory

    return list_all(human_readable);
}
