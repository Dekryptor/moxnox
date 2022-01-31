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
    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw->get();

    if (obj.type != object_type::file) {
        visit_obj(obj, [](auto&& o) {
            o.release();
        });

        return nt_status::INVALID_HANDLE;
    }

    auto& f = static_cast<file&>(obj);

    auto ret = f.query_directory(event, apc_routine, apc_context, io_status_block, file_information,
                                 length, file_information_class, return_single_entry,
                                 file_name, restart_scan);

    f.release();

    return ret;
}

NTSTATUS file::query_directory(HANDLE event, PIO_APC_ROUTINE apc_routine,
                               void* apc_context, IO_STATUS_BLOCK* io_status_block,
                               void* file_information, uint32_t length,
                               FILE_INFORMATION_CLASS file_information_class, bool return_single_entry,
                               const UNICODE_STRING* file_name, bool restart_scan) {
    print("FIXME - file::query_directory\n");

    // FIXME

    return nt_status::NOT_IMPLEMENTED;
}
