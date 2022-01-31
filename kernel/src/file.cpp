#include "kernel.h"
#include "objects.h"

using namespace std;

size_t file::read(span<uint8_t> s) {
    auto ret = ino.read(s, file_offset);

    if (ret > 0) {
        lock_guard l(lock);
        file_offset += ret;
    }

    return ret;
}

size_t file::read(span<uint8_t> s, uint64_t offset) {
    auto ret = ino.read(s, offset);

    if (ret > 0) {
        lock_guard l(lock);
        file_offset = offset + ret;
    }

    return ret;
}

NTSTATUS NtQueryDirectoryFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                              void* apc_context, IO_STATUS_BLOCK* io_status_block,
                              void* file_information, uint32_t length,
                              FILE_INFORMATION_CLASS file_information_class, bool return_single_entry,
                              const UNICODE_STRING* file_name, bool restart_scan) {
    print("FIXME - NtQueryDirectoryFile\n");

    return nt_status::NOT_IMPLEMENTED;
}
