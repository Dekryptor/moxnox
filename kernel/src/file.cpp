#include "kernel.h"
#include <span>
#include "handle.h"
#include "spinlock.h"
import objects;
import page_cache;
import rwsem;
#include "inode.h"
#include "file.h"
import visit;

using namespace std;

file::file(object& ino, uint32_t create_options) : object(object_type::file), ino(ino), create_options(create_options) {
    object_acquire(ino);
}

file::~file() {
    object_release(ino);

    free(name);
}

NTSTATUS file::read(span<uint8_t> s, uint64_t& read_bytes) {
    NTSTATUS status;

    status = object_read_without_offset(ino, s, read_bytes);
    if (status != nt_status::NOT_SUPPORTED)
        return status;

    status = object_read_with_offset(ino, s, file_offset, read_bytes);

    if (success(status)) {
        lock_guard l(lock);
        file_offset += read_bytes;
    }

    return status;
}

NTSTATUS file::read(span<uint8_t> s, uint64_t offset, uint64_t& read_bytes) {
    auto status = object_read_with_offset(ino, s, offset, read_bytes);

    if (success(status)) {
        lock_guard l(lock);
        file_offset = offset + read_bytes;
    }

    return status;
}

NTSTATUS file::write(span<const uint8_t> s, uint64_t& written_bytes) {
    NTSTATUS status;

    // FIXME - update file position if FILE_SYNCHRONOUS_IO_ALERT or FILE_SYNCHRONOUS_IO_NONALERT set

    status = object_write_without_offset(ino, s, written_bytes);
    if (status != nt_status::NOT_SUPPORTED)
        return status;

    status = object_write_with_offset(ino, s, file_offset, written_bytes);

    if (success(status) && written_bytes > 0) {
        lock_guard l(lock);
        file_offset += written_bytes;
    }

    return status;
}

NTSTATUS file::write(span<const uint8_t> s, uint64_t offset, uint64_t& written_bytes) {
    // FIXME - if FILE_SYNCHRONOUS_IO_ALERT or FILE_SYNCHRONOUS_IO_NONALERT set, set file_offset to offset first

    auto status = object_write_with_offset(ino, s, offset, written_bytes);

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
    auto objrw = deref_handle(file_handle);
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

NTSTATUS NtSetInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* iosb, void* buffer,
                              uint32_t buffer_size, FILE_INFORMATION_CLASS file_information_class) {
    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    iosb->Information = 0;

    return f.set_information(buffer, buffer_size, file_information_class);
}

NTSTATUS file::set_information(void* buffer, uint32_t buffer_size,
                               FILE_INFORMATION_CLASS file_information_class) {
    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    return static_cast<inode&>(ino).set_information(span((uint8_t*)buffer, buffer_size),
                                                    file_information_class);
}

NTSTATUS NtQueryInformationFile(HANDLE file_handle, IO_STATUS_BLOCK* iosb, void* buffer,
                                uint32_t buffer_size, FILE_INFORMATION_CLASS file_information_class) {
    uint64_t retlen;

    auto objrw = deref_handle(file_handle);
    if (!objrw)
        return nt_status::INVALID_HANDLE;

    auto& obj = objrw.get();

    if (obj.type != object_type::file)
        return nt_status::INVALID_HANDLE;

    auto& f = static_cast<file&>(obj);

    auto ret = f.query_information(buffer, buffer_size, file_information_class,
                                   retlen);

    if (!success(ret)) {
        iosb->Information = 0;
        return ret;
    }

    iosb->Information = retlen;

    return ret;
}

NTSTATUS file::query_information(void* buffer, uint32_t buffer_size,
                                 FILE_INFORMATION_CLASS file_information_class,
                                 uint64_t& retlen) {
    if (ino.type != object_type::inode)
        return nt_status::INVALID_PARAMETER;

    return static_cast<inode&>(ino).query_information(span((uint8_t*)buffer, buffer_size),
                                                      file_information_class, retlen);
}
