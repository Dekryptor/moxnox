#include "kernel.h"
#include "objects.h"
#include "handle.h"

using namespace std;

NTSTATUS file::read(span<uint8_t> s, uint64_t& read_bytes) {
    NTSTATUS status;

    visit_obj(ino, [&](auto&& o) {
        if constexpr (is_readable_object_without_offset<remove_reference_t<decltype(o)>>) {
            status = o.read(s, read_bytes);
        } else if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>) {
            status = o.read(s, file_offset, read_bytes);

            if (success(status)) {
                lock_guard l(lock);
                file_offset += read_bytes;
            }
        } else
            status = nt_status::INTERNAL_ERROR;
    });

    return status;
}

NTSTATUS file::read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) {
    NTSTATUS status;

    visit_obj(ino, [&](auto&& o) {
        if constexpr (is_readable_object_with_offset<remove_reference_t<decltype(o)>>)
            status = o.read(s, offset, read_bytes);
        else
            status = nt_status::INTERNAL_ERROR;
    });

    if (success(status)) {
        lock_guard l(lock);
        file_offset = offset + read_bytes;
    }

    return status;
}

NTSTATUS file::write(span<const uint8_t> s, uint64_t& written_bytes) {
    NTSTATUS status;

    visit_obj(ino, [&](auto&& o) {
        if constexpr (is_writable_object_without_offset<remove_reference_t<decltype(o)>>) {
            status = o.write(s, written_bytes);
        } else if constexpr (is_writable_object_with_offset<remove_reference_t<decltype(o)>>) {
            status = o.write(s, file_offset, written_bytes);

            if (success(status) && written_bytes > 0) {
                lock_guard l(lock);
                file_offset += written_bytes;
            }
        } else
            status = nt_status::INTERNAL_ERROR;
    });

    return status;
}

NTSTATUS file::write(span<const uint8_t> s, uint64_t offset, uint64_t& written_bytes) {
    NTSTATUS status;

    visit_obj(ino, [&](auto&& o) {
        if constexpr (is_writable_object_with_offset<remove_reference_t<decltype(o)>>)
            status = o.write(s, offset, written_bytes);
        else
            status = nt_status::INTERNAL_ERROR;
    });

    if (success(status) && written_bytes > 0) {
        lock_guard l(lock);
        file_offset = offset + written_bytes;
    }

    return status;
}

NTSTATUS NtQueryDirectoryFile(HANDLE file_handle, HANDLE event, PIO_APC_ROUTINE apc_routine,
                              void* apc_context, IO_STATUS_BLOCK* io_status_block,
                              void* file_information, uint32_t length,
                              FILE_INFORMATION_CLASS file_information_class, bool return_single_entry,
                              const UNICODE_STRING* file_name, bool restart_scan) {
    auto objrw = deref_handle2(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    auto ret = f.query_directory(event, apc_routine, apc_context, *io_status_block, file_information,
                                 length, file_information_class, return_single_entry,
                                 file_name, restart_scan);

    return ret;
}

NTSTATUS file::query_directory(HANDLE, PIO_APC_ROUTINE, void*, IO_STATUS_BLOCK& io_status_block,
                               void* file_information, uint32_t length,
                               FILE_INFORMATION_CLASS file_information_class, bool,
                               const UNICODE_STRING*, bool restart_scan) {
    // FIXME - event
    // FIXME - APC
    // FIXME - filename

    if (restart_scan)
        query_dir_index = 0;

    // FIXME - returning multiple entries

    auto index = query_dir_index;
    auto sp = span((uint8_t*)file_information, length);

    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    auto ret = static_cast<inode&>(ino).query_directory(sp, file_information_class, index);

    if (!success(ret))
        return ret;

    query_dir_index = index;
    io_status_block.Information = sp.size();

    return ret;
}
