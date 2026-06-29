#include <sys/mman.h>
#include <errno.h>

import nt;
import unique_handle;
import ucrtbase;

#undef errno
import errno;

extern "C"
void* mmap(void* /*addr*/, size_t len, int prot, int flags, int fd, off_t off) {
    nt_status status;
    LARGE_INTEGER lisize, lioff;
    HANDLE file, h;
    uint32_t allocation_attributes, page_protection;
    size_t view_size;
    void* base_address;

    // FIXME - MAP_FIXED
    // FIXME - does PROT_NONE work?

    if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) {
        *_errno() = EINVAL;
        return MAP_FAILED;
    }

    if (flags & ~(MAP_FIXED | MAP_ANONYMOUS | MAP_PRIVATE | MAP_SHARED)) {
        *_errno() = EINVAL;
        return MAP_FAILED;
    }

    if (prot & PROT_WRITE && prot & PROT_EXEC)
        page_protection = flags & MAP_SHARED ? PAGE_EXECUTE_READWRITE : PAGE_EXECUTE_WRITECOPY;
    else if (prot & PROT_EXEC)
        page_protection = PAGE_EXECUTE_READ;
    else if (prot & PROT_WRITE)
        page_protection = flags & MAP_SHARED ? PAGE_READWRITE : PAGE_WRITECOPY;
    else if (prot & PROT_READ)
        page_protection = PAGE_READONLY;
    else
        page_protection = PAGE_NOACCESS;

    if (!(flags & MAP_ANONYMOUS)) {
        file = (HANDLE)_get_osfhandle(fd);
        if (file == (HANDLE)-1) {
            *_errno() = EINVAL;
            return MAP_FAILED;
        }

        allocation_attributes = SEC_FILE;
    } else {
        file = 0;
        allocation_attributes = SEC_COMMIT;
    }

    lisize.QuadPart = len + off;

    // FIXME - access mask
    status = NtCreateSection(&h, 0, nullptr, &lisize, page_protection,
                             allocation_attributes, file);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return MAP_FAILED;
    }

    unique_handle sect{h};

    lioff.QuadPart = off;
    view_size = len;
    base_address = nullptr;

    status = NtMapViewOfSection(sect.get(), NtCurrentProcess(), &base_address,
                                0, 0, &lioff, &view_size, SECTION_INHERIT::ViewUnmap,
                                0, page_protection);
    if (!success(status)) {
        *_errno() = nt_status_to_errno(status);
        return MAP_FAILED;
    }

    return base_address;
}

extern "C"
int munmap(void* addr, size_t /*len*/) {
    // FIXME - partial unmapping

    auto status = NtUnmapViewOfSection(NtCurrentProcess(), addr);

    // if map not there, POSIX says we should return 0
    if (!success(status) && status != nt_status::NO_MEMORY) {
        *_errno() = nt_status_to_errno(status);
        return -1;
    }

    return 0;
}
